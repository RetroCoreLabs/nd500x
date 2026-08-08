#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Mulad instruction - ARITHMETIC class
 *
 * Multiply and Add: Rn * <x> + <y> → Rn
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn MULAD, Hn MULAD, Wn MULAD, Fn MULAD, Dn MULAD (n=1..4)
 * Operands: 2 (<x/r/t>, <y/r/t>)
 *
 * Opcodes:
 *   0xFCE8-0xFCEB (BY1 MULAD through BY4 MULAD) - Byte multiply and add
 *   0xFCEC-0xFCEF (H1 MULAD through H4 MULAD) - Halfword multiply and add
 *   0x00A8-0x00AB (W1 MULAD through W4 MULAD) - Word multiply and add
 *   0xFCF0-0xFCF3 (F1 MULAD through F4 MULAD) - Float multiply and add
 *   0xFCF4-0xFCF7 (D1 MULAD through D4 MULAD) - Double multiply and add
 *
 * Operation: Rn * <x> + <y> → Rn
 *
 * Description:
 *   The register contents are multiplied by <x>, <y> is added to
 *   the product, and the result is stored in the register.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow), FO, FU
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if carry from most significant bit (integer word only)
 *   O = 1 if overflow
 *   FO = 1 if floating overflow
 *   FU = 1 if floating underflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11.19
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Mulad.cs
 */
void nd500_instr_Mulad(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 53-57) */
    if (fi->operand_count != 2) {
        printf("[ERROR] MULAD at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double types.
     * Microcode MULADF @002633 / MULADD @002635: Rn * <x> + <y> -> Rn;
     * ST,SAVF/ST,ACCF set Z,S,FU,FO; C,O cleared (rule 4040). */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        /* Read Rn as ND-500 NATIVE float/double (bias-256) and CONVERT to IEEE for
         * arithmetic, matching the native reference path (Add.c) and the now-native
         * nd500_read_operand_as_ieee_float helper. Reference: ND-500 Reference Manual
         * sections 2.5.1.4 / 2.5.3.6. */
        double regValue;
        if (is_double) {
            regValue = nd500_native_double_to_double(nd500_read_double_register(cpu, fi->target_register));
        } else {
            regValue = nd500_native_single_to_double(nd500_read_float_register(cpu, fi->target_register));
        }
        double x = nd500_read_operand_as_ieee_float(cpu, &fi->operands[0], is_double);
        double y = nd500_read_operand_as_ieee_float(cpu, &fi->operands[1], is_double);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        double fresult = regValue * x + y;
        uint64_t bits = nd500_float_finish(cpu, fi->address, fresult, is_double);
        if (is_double) {
            nd500_write_double_register(cpu, fi->target_register, bits);
        } else {
            nd500_write_float_register(cpu, fi->target_register, (uint32_t)bits);
        }
        return;
    }

    /* Integer multiply and add (like C# lines 145-202) */

    /* Read register value (like C# line 146) */
    uint32_t regValue = nd500_read_integer_register(cpu, fi->target_register);

    /* Read operand x (like C# line 147) */
    uint64_t x = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Read operand y (like C# line 148) */
    uint64_t y = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Perform: Rn * x + y, MODELLING THE HARDWARE'S TWO SEPARATE STEPS.
     *
     * The product is TRUNCATED TO THE DATA-TYPE WIDTH BEFORE the addend is added,
     * because that is what the machine does: MICRO-5800-B30 MULADW (002627..002631)
     * issues AAP2,IMUL with the product landing in the 32-bit scratch SC7, saves the
     * multiply status with ST,SAVM, and only then adds - ALU,A+B A,SC7 B,SC5.
     * Computing it all in wider arithmetic and truncating once at the end gives a
     * different carry.
     *
     * CARRY IS THE ADD'S CARRY-OUT, NOT "the true result exceeded the width".
     * [CORRECTED 2026-07-25 - this was carry = (temp > 0xFFFFFFFF) over the
     * full-precision product+addend, which attributes the MULTIPLY excess to C.
     * The ND-500 Reference Manual settles it by comparison:
     *     11.1  ADD   - "carry from most significant bit -> C (integer)"   C present
     *     11.7  MUL   - Z, S, O, FU, FO only                              C ABSENT
     *     11.19 MULAD - "carry from most significant bit -> C (integer)"   C present
     * MULAD is MUL followed by ADD; C is absent from MUL, present in ADD, and appears
     * in MULAD with ADD's exact wording - so C comes from the ADD. The multiply excess
     * is already reported through O, so folding it into C double-counted it. The old
     * form also never set C at all for BY/H, though the manual scopes C to every
     * integer type. Confirmed against the real B30 microcode by the differential
     * oracle. Mirrors the C# fix in
     * Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Mulad.cs. */
    uint64_t result = 0;
    bool overflow = false;
    bool carry = false;
    int width;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:     width = 1; break;
        case ND500_DTYPE_HALFWORD: width = 2; break;
        case ND500_DTYPE_WORD:     width = 4; break;
        default:
            printf("[ERROR] MULAD at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    {
        int type_bits = width * 8;
        uint64_t width_mask = ((uint64_t)1 << type_bits) - 1;

        /* Sign-extend the three inputs from their data-type width. */
        int64_t reg  = nd500_sign_extend_by_dtype(regValue, fi->data_type);
        int64_t xVal = nd500_sign_extend_by_dtype(x, fi->data_type);
        int64_t yVal = nd500_sign_extend_by_dtype(y, fi->data_type);

        /* Step 1: multiply. Overflow if the true product does not fit the SIGNED
         * width - the AAP multiply-overflow latch that ST,SAVM captures. */
        int64_t true_product = reg * xVal;
        int64_t signed_min = -((int64_t)1 << (type_bits - 1));
        int64_t signed_max = ((int64_t)1 << (type_bits - 1)) - 1;
        bool mul_overflow = (true_product < signed_min || true_product > signed_max);

        /* The truncated product is what actually reaches the adder. */
        uint64_t product = (uint64_t)true_product & width_mask;
        uint64_t addend  = (uint64_t)yVal & width_mask;

        /* Step 2: add at the data-type width. C is the carry OUT of the most
         * significant bit - a property of this binary addition, independent of
         * whether the true value fit. */
        uint64_t sum = product + addend;
        carry = ((sum & ~width_mask) != 0);
        result = sum & width_mask;

        /* Signed overflow of the ADD: both addends share a sign that differs from
         * the result sign. */
        int64_t signed_product = nd500_sign_extend_by_dtype(product, fi->data_type);
        int64_t signed_sum = nd500_sign_extend_by_dtype(result, fi->data_type);
        bool add_overflow = ((signed_product < 0) == (yVal < 0)) &&
                            ((signed_sum < 0) != (signed_product < 0));

        /* O is STICKY across the two steps: ST,SAVM saves the multiply status and
         * ST,ACCA then ACCUMULATES the add status, so an overflow in EITHER shows. */
        overflow = mul_overflow || add_overflow;
    }

    /* Mask and write result (like C# lines 194-195) */
    uint32_t maskedResult = nd500_mask_to_datatype((uint32_t)result, fi->data_type);
    nd500_write_integer_register(cpu, fi->target_register, maskedResult);

    /* Update status flags (like C# lines 198-201) */
    /* Set Z flag based on result */
    if (maskedResult == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    /* Set S flag based on sign bit */
    bool signBit = false;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            signBit = ((maskedResult & 0x80) != 0);
            break;
        case ND500_DTYPE_HALFWORD:
            signBit = ((maskedResult & 0x8000) != 0);
            break;
        case ND500_DTYPE_WORD:
            signBit = ((maskedResult & 0x80000000) != 0);
            break;
    }

    if (signBit) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }

    /* Set C flag - carry out of the add MSB, for EVERY integer width */
    if (carry) {
        cpu->ST1 |= ND500_FLAG_C;
    } else {
        cpu->ST1 &= ~ND500_FLAG_C;
    }

    /* Set O flag based on overflow */
    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    /* Handle trap on overflow */
    if (overflow) {
        ND500X_TRAPLOG("[TRAP] MULAD at PC=0x%08X: Integer overflow\n", fi->address);
        trap_integer_overflow(cpu, fi->address);
        return;
    }
}
