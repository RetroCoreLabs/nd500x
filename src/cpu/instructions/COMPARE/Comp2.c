#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Comp2 instruction - COMPARE class
 *
 * Compare two operands (subtract second from first, discard result).
 * op1 - op2 (result discarded, flags updated)
 *
 * Variants: 6 (by data type)
 * Mnemonics: BI COMP2, BY COMP2, H COMP2, W COMP2, F COMP2, D COMP2
 * Operands: 2 (operand1, operand2)
 *
 * Opcodes:
 *   0xFC15 (BI COMP2) - Bit compare
 *   0x002D (BY COMP2) - Byte compare
 *   0xFC16 (H COMP2) - Halfword compare
 *   0x002E (W COMP2) - Word compare
 *   0x002F (F COMP2) - Float compare (NOT IMPLEMENTED)
 *   0x0040 (D COMP2) - Double compare (NOT IMPLEMENTED)
 *
 * Operation: op1 - op2 (result not stored)
 *
 * Description:
 *   The compare two operands instruction subtracts the second operand
 *   from the first. The result sets the data status bits accordingly,
 *   but the result is otherwise discarded.
 *
 * Flags: Z (zero), S (sign XOR overflow), C (carry/borrow)
 *   Z = 1 if result is zero (operands are equal)
 *   S = sign_bit XOR overflow (true comparison)
 *   C = 1 if NO borrow (op1 >= op2 for unsigned)
 *
 * Trap conditions: Addressing traps, Floating underflow (FU), Floating overflow (FO)
 *
 * Reference: ND-500 Reference Manual, Chapter 10.10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/COMPARE/Comp2.cs
 */
void nd500_instr_Comp2(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] COMP2 at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Floating compares (F/D COMP2) must compare as FLOATS, not raw integer bit
     * patterns. Doing integer subtraction on ND float bits sets S/C (and Z for
     * some encodings, e.g. +0.0 vs -0.0) wrong, which flips the conditional branch
     * that consumes them - e.g. the freelist size-bucket rounding at 0x0802CED9
     * (d comp2 / if = go). Compare the decoded IEEE values instead. */
    if (fi->data_type == ND500_DTYPE_FLOAT || fi->data_type == ND500_DTYPE_DOUBLEWORD) {
        double a, b;
        if (fi->data_type == ND500_DTYPE_DOUBLEWORD) {
            a = nd500_double_to_ieee754(nd500_read_operand_doubleword(cpu, &fi->operands[0]));
            b = nd500_double_to_ieee754(nd500_read_operand_doubleword(cpu, &fi->operands[1]));
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
        }
        } else {
            a = (double)nd500_float_to_ieee754((uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT));
            b = (double)nd500_float_to_ieee754((uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_FLOAT));
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }
        }
        {   /* ND500X_FCMPDBG=1: log every float/double compare with the
             * decoded values and operand modes.  This is what showed that
             * libc's iszero_d() was comparing an un-negated -8.5 - the trail
             * that led to the swapped A/E double-register halves. */
            static int on = -1;
            if (on < 0) { const char* e = getenv("ND500X_FCMPDBG"); on = (e && e[0] && e[0] != '0') ? 1 : 0; }
            if (on) {
                static unsigned n = 0;
                if (n++ < 200)
                    fprintf(stderr, "[FCMP] PC=0x%08X CED=%u a=%g (mode=%u reg=%u) b=%g (mode=%u reg=%u)\n",
                            fi->address, cpu->CED, a, fi->operands[0].mode, fi->operands[0].reg,
                            b, fi->operands[1].mode, fi->operands[1].reg);
            }
        }
        /* op1 - op2: Z = equal, S = op1 < op2 (result sign), C = no borrow (op1 >= op2) */
        if (a == b) nd500_set_flag(cpu, ND500_FLAG_Z); else nd500_clear_flag(cpu, ND500_FLAG_Z);
        if (a <  b) nd500_set_flag(cpu, ND500_FLAG_S); else nd500_clear_flag(cpu, ND500_FLAG_S);
        if (a >= b) nd500_set_flag(cpu, ND500_FLAG_C); else nd500_clear_flag(cpu, ND500_FLAG_C);
        return;
    }

    /* Read both operands (like C# lines 49-50) */
    uint64_t op1 = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    uint64_t op2 = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Normalise both operands to the datatype width before comparing. The
     * operand fetch may deliver the two operands with different extensions - a
     * memory halfword zero-extended (0x0000FFFF) while an immediate -1 is
     * sign-extended (0xFFFFFFFFFFFFFFFF). Comparing the raw 64-bit values then
     * yields a non-zero difference for values that are equal within the
     * datatype (e.g. H 0xFFFF vs -1), wrongly clearing Z. The ND-500 compares
     * two operands OF THE GIVEN DATATYPE, so mask to that width first. */
    uint64_t width_mask;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:       width_mask = 0xFFull; break;
        case ND500_DTYPE_HALFWORD:   width_mask = 0xFFFFull; break;
        case ND500_DTYPE_WORD:       width_mask = 0xFFFFFFFFull; break;
        default:                     width_mask = 0xFFFFFFFFFFFFFFFFull; break;
    }
    op1 &= width_mask;
    op2 &= width_mask;

    /* BI (bit) COMPARE: isolate BOTH operands to bit 0. The width_mask default leaves BI
     * full-width, so a register/operand like 0xAAAAAAAA would compare its whole width instead of
     * just its LSB. Mirrors RetroCore Comp2.cs (DataTypeWidthMask(BI)=0x01). The subtraction below
     * keeps the FULL width (result byte sign) so the true-comparison sign works. */
    if (fi->data_type == ND500_DTYPE_BIT) { op1 &= 0x1ull; op2 &= 0x1ull; }

    /* Perform subtraction (result not stored) (like C# line 53) */
    uint64_t result = (op1 - op2) & width_mask;

    /* ND-500 carry convention: C=1 means NO borrow (op1 >= op2)
     * Reference: ND-500 Reference Manual Page 2040 */
    bool carry = (op1 >= op2);

    /* Detect overflow (like C# line 57) */
    /* For subtraction overflow: overflow occurs when:
     * - Subtracting positive from negative gives positive (sign flip)
     * - Subtracting negative from positive gives negative (sign flip) */
    bool overflow = false;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            int8_t s_op1 = (int8_t)op1;
            int8_t s_op2 = (int8_t)op2;
            int8_t s_result = (int8_t)result;
            overflow = ((s_op1 >= 0 && s_op2 < 0 && s_result < 0) ||
                       (s_op1 < 0 && s_op2 >= 0 && s_result >= 0));
            break;
        }
        case ND500_DTYPE_HALFWORD: {
            int16_t s_op1 = (int16_t)op1;
            int16_t s_op2 = (int16_t)op2;
            int16_t s_result = (int16_t)result;
            overflow = ((s_op1 >= 0 && s_op2 < 0 && s_result < 0) ||
                       (s_op1 < 0 && s_op2 >= 0 && s_result >= 0));
            break;
        }
        case ND500_DTYPE_WORD: {
            int32_t s_op1 = (int32_t)op1;
            int32_t s_op2 = (int32_t)op2;
            int32_t s_result = (int32_t)result;
            overflow = ((s_op1 >= 0 && s_op2 < 0 && s_result < 0) ||
                       (s_op1 < 0 && s_op2 >= 0 && s_result >= 0));
            break;
        }
        default:
            overflow = false;
            break;
    }

    /* Update Z flag (like C# line 60) */
    if (result == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    /* Update C flag (like C# line 61) */
    if (carry) {
        nd500_set_flag(cpu, ND500_FLAG_C);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_C);
    }

    /* Get sign bit (like C# lines 64-73) */
    bool sign_bit = false;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:      sign_bit = (result & 0x80) != 0; break;
        case ND500_DTYPE_HALFWORD:  sign_bit = (result & 0x8000) != 0; break;
        case ND500_DTYPE_WORD:      sign_bit = (result & 0x80000000ULL) != 0; break;
        case ND500_DTYPE_DOUBLEWORD: sign_bit = (result & 0x8000000000000000ULL) != 0; break;
        /* BI COMPARE sign = BYTE sign of the raw single-bit diff op1-op2 (microword COMP2_BI
         * @003241 subtracts TYP,BY): -1 -> byte 0xFF -> S=1=(op1<op2); +1 -> 0x01 -> S=0. */
        case ND500_DTYPE_BIT:       sign_bit = (result & 0x80) != 0; break;
    }

    /* S = sign_bit XOR overflow (like C# line 74) */
    bool s_flag = sign_bit ^ overflow;
    if (s_flag) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }
}
