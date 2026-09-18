#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * Add2 instruction - ARITHMETIC class
 *
 * Extended Add (Two Operands): <a> + Rn -> <b>
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn ADD2, Hn ADD2, Wn ADD2, Fn ADD2, Dn ADD2 (n=1..4)
 * Operands: 2 (<a/r/t>, <b/w/t>)
 *
 * Opcodes:
 *   0xFC3C-0xFC3F (BY1 ADD2 through BY4 ADD2) - Byte extended add
 *   0xFC40-0xFC43 (H1 ADD2 through H4 ADD2) - Halfword extended add
 *   0x0060-0x0063 (W1 ADD2 through W4 ADD2) - Word extended add
 *   0x0064-0x0067 (F1 ADD2 through F4 ADD2) - Float extended add
 *   0x0068-0x006B (D1 ADD2 through D4 ADD2) - Double extended add
 *
 * Operation: <a> + Rn -> <b>
 *
 * Description:
 *   The <a> operand is added to the contents of the specified register.
 *   The result is stored in the <b> operand location. This is an extended
 *   version of the basic ADD instruction that allows storing the result
 *   to a memory location rather than back to the register.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if carry from most significant bit (integer only)
 *   O = 1 if overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Extended Arithmetic)
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Add2.cs
 */
void nd500_instr_Add2(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 54-58) */
    if (fi->operand_count != 2) {
        printf("[ERROR] ADD2 at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double types (like C# lines 105-139) */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        bool overflow = false;

        if (is_double) {
            /* Double precision (D ADD2): a + b -> a */
            uint64_t a_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
            uint64_t b_bits = nd500_read_operand_doubleword(cpu, &fi->operands[1]);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }

            double a_ieee = nd500_double_to_ieee754(a_bits);
            double b_ieee = nd500_double_to_ieee754(b_bits);
            double sum = a_ieee + b_ieee;

            /* Check for overflow/underflow */
            if (isinf(sum)) {
                overflow = true;
                trap_floating_overflow(cpu, fi->address);
            } else if (sum != 0.0 && fabs(sum) < 1e-308) {
                /* Underflow - result too small to represent */
                trap_floating_underflow(cpu, fi->address);
            }

            /* Convert back to ND-500 format */
            uint64_t result_bits = nd500_double_from_ieee754(sum);

            /* Write result back to operand a */
            nd500_write_operand_value(cpu, &fi->operands[0], result_bits, ND500_DTYPE_DOUBLEWORD);

            /* Update flags */
            if (nd500_double_is_zero(result_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_Z);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_Z);
            }
            if (nd500_double_is_negative(result_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_S);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_S);
            }
        } else {
            /* Single precision (F ADD2): a + b -> a */
            uint32_t a_bits = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
            uint32_t b_bits = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }

            float a_ieee = nd500_float_to_ieee754(a_bits);
            float b_ieee = nd500_float_to_ieee754(b_bits);
            float sum = a_ieee + b_ieee;

            /* Check for overflow/underflow */
            if (isinf(sum)) {
                overflow = true;
                trap_floating_overflow(cpu, fi->address);
            } else if (sum != 0.0f && fabsf(sum) < 1e-38f) {
                /* Underflow - result too small to represent */
                trap_floating_underflow(cpu, fi->address);
            }

            /* Convert back to ND-500 format */
            uint32_t result_bits = nd500_float_from_ieee754(sum);

            /* Write result back to operand a */
            nd500_write_operand_value(cpu, &fi->operands[0], (uint64_t)result_bits, ND500_DTYPE_WORD);

            /* Update flags */
            if (nd500_float_is_zero(result_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_Z);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_Z);
            }
            if (nd500_float_is_negative(result_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_S);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_S);
            }
        }

        /* C flag unaffected for float, O flag set based on overflow */
        if (overflow) {
            nd500_set_flag(cpu, ND500_FLAG_O);
            trap_floating_overflow(cpu, fi->address);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_O);
        }

        return;
    }

    uint64_t aValue, bValue, result;
    bool overflow = false;
    bool carry = false;

    /* Read operand a (first operand - destination and source) - C# line 60 */
    aValue = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Read operand b (second operand - source only) - C# line 63 */
    bValue = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Perform addition: a + b (like C# lines 66-103) */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            /* Signed byte addition (like C# lines 73-81) */
            int8_t aByte = (int8_t)(aValue & 0xFF);
            int8_t bByte = (int8_t)(bValue & 0xFF);
            int32_t sum = (int32_t)aByte + (int32_t)bByte;
            result = (uint64_t)(uint8_t)(sum & 0xFF);
            overflow = (sum < -128 || sum > 127);
            /* Carry is UNSIGNED overflow of the operands - the signed sum
             * sign-extends and fakes a carry for negative results (the Add3
             * fuword/copyout _Udata bug, same class). */
            carry = ((((uint32_t)(uint8_t)aByte + (uint32_t)(uint8_t)bByte) & 0x100u) != 0);
            break;
        }

        case ND500_DTYPE_HALFWORD: {
            /* Signed halfword addition (like C# lines 84-92) */
            int16_t aHalf = (int16_t)(aValue & 0xFFFF);
            int16_t bHalf = (int16_t)(bValue & 0xFFFF);
            int32_t sum = (int32_t)aHalf + (int32_t)bHalf;
            result = (uint64_t)(uint16_t)(sum & 0xFFFF);
            overflow = (sum < -32768 || sum > 32767);
            carry = ((((uint32_t)(uint16_t)aHalf + (uint32_t)(uint16_t)bHalf) & 0x10000u) != 0);
            break;
        }

        case ND500_DTYPE_WORD: {
            /* Signed word addition (like C# lines 95-103) */
            int32_t aWord = (int32_t)(aValue & 0xFFFFFFFF);
            int32_t bWord = (int32_t)(bValue & 0xFFFFFFFF);
            int64_t sum = (int64_t)aWord + (int64_t)bWord;
            result = (uint64_t)(uint32_t)(sum & 0xFFFFFFFF);
            overflow = (sum < INT32_MIN || sum > INT32_MAX);
            carry = ((((uint64_t)(uint32_t)aWord + (uint64_t)(uint32_t)bWord) & 0x100000000ULL) != 0);
            break;
        }

        default:
            printf("[ERROR] ADD2 at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Write result to operand a (destination) - C# line 143 */
    nd500_write_operand_value(cpu, &fi->operands[0], result, fi->data_type);

    /* Update status flags based on result (like C# lines 146-150) */
    nd500_set_flags_zsco(cpu, result, fi->data_type, carry, overflow);

    /* Handle trap conditions (like C# lines 152-164) */
    if (overflow) {
        ND500X_TRAPLOG("[TRAP] ADD2 at PC=0x%08X: Integer overflow\n", fi->address);
        trap_integer_overflow(cpu, fi->address);
        return;
    }
}
