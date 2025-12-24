#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

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
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/COMPARE/Comp2.cs
 */
void nd500_instr_Comp2(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] COMP2 at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read both operands (like C# lines 49-50) */
    uint64_t op1 = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    uint64_t op2 = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* DEBUG: trace comparison values */
    TRACE("[COMP2] op1=0x%llX op2=0x%llX\n", (unsigned long long)op1, (unsigned long long)op2);

    /* Perform subtraction (result not stored) (like C# line 53) */
    uint64_t result = op1 - op2;

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
    }

    /* S = sign_bit XOR overflow (like C# line 74) */
    bool s_flag = sign_bit ^ overflow;
    TRACE("[COMP2] result=0x%llX sign=%d ovf=%d S=%d C=%d\n",
          (unsigned long long)result, sign_bit, overflow, s_flag, carry);
    if (s_flag) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }
}
