#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * Subtract instruction - ARITHMETIC class
 *
 * Subtract from Register: Rn - <operand> → Rn
 *
 * Variants: 5
 * Mnemonics: - (subtract)
 * Operands: 1 (<operand/r/t>)
 *
 * Opcodes:
 *   0xFC3C (BYn -) byte subtract
 *   0xFC40 (Hn -)  halfword subtract
 *   0x0060 (Wn -)  word subtract
 *   0x0064 (Fn -)  float subtract
 *   0x0068 (Dn -)  double subtract
 *
 * Operation: Rn - <operand> → Rn
 *
 * Description:
 *   The operand is subtracted from the contents of the specified register.
 *   The result is stored in the register.
 *
 *   Register selection is encoded in opcode bits 1-0:
 *   - 00 → register 1 (I1, A1)
 *   - 01 → register 2 (I2, A2)
 *   - 10 → register 3 (I3, A3)
 *   - 11 → register 4 (I4, A4)
 *
 *   For integer variants: Uses I1-I4 registers
 *   For float/double: Uses A1-A4 (float) or D1-D4 (A+E pairs, double)
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if difference is zero
 *   S = 1 if sign bit is set
 *   C = 1 if NO borrow (minuend >= subtrahend)
 *   O = 1 if overflow (integer or float overflow)
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Basic Arithmetic)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Subtract.cs
 */
void nd500_instr_Subtract(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count and target register using helper functions
    if (!nd500_validate_operand_count(cpu, fi, 1, INSTR_SUBTRACT)) return;
    if (!nd500_validate_target_register(cpu, fi, INSTR_SUBTRACT)) return;

    /* Handle float/double variants */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        uint8_t reg_num = fi->target_register;

        if (reg_num < 1 || reg_num > 4) {
            printf("[ERROR] SUBTRACT at PC=0x%08X: Invalid register %u\n",
                   fi->address, reg_num);
            trap_illegal_operand(cpu, fi->address);
            return;
        }

        /* Read register value */
        double reg_value = 0.0;
        if (is_double) {
            uint64_t reg_bits = nd500_read_double_register(cpu, reg_num);
            reg_value = nd500_double_to_ieee754(reg_bits);
        } else {
            uint32_t reg_bits = nd500_read_float_register(cpu, reg_num);
            reg_value = (double)nd500_float_to_ieee754(reg_bits);
        }

        /* Read operand value */
        double operand_value = 0.0;
        if (is_double) {
            uint64_t op_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
            operand_value = nd500_double_to_ieee754(op_bits);
        } else {
            uint32_t op_bits = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
            operand_value = (double)nd500_float_to_ieee754(op_bits);
        }

        /* Perform subtraction */
        double result = reg_value - operand_value;

        /* Check for overflow/underflow */
        if (isinf(result)) {
            trap_floating_overflow(cpu, fi->address);
        }

        /* Convert result back to ND-500 format and write to register */
        if (is_double) {
            uint64_t result_bits = nd500_double_from_ieee754(result);
            nd500_write_double_register(cpu, reg_num, result_bits);

            /* Update flags: Z (zero), S (sign) */
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
            uint32_t result_bits = nd500_float_from_ieee754((float)result);
            nd500_write_float_register(cpu, reg_num, result_bits);

            /* Update flags: Z (zero), S (sign) */
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
        return;
    }

    // Integer subtraction
    uint32_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t operand = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    // Perform subtraction
    uint64_t result = reg_value - operand;

    /* ND-500 carry convention: C=1 means NO borrow (minuend >= subtrahend)
     * Reference: ND-500 Reference Manual Page 2040, SUBC formula Page 194 */
    bool carry = (reg_value >= operand);

    // Detect overflow
    bool overflow = nd500_detect_sub_overflow(reg_value, operand, result, fi->data_type);

    // Mask result to data type
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    // Write back to register
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    // Update status flags
    if (masked_result == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    if (carry) {
        cpu->ST1 |= ND500_FLAG_C;
    } else {
        cpu->ST1 &= ~ND500_FLAG_C;
    }

    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    // Set sign bit based on data type
    bool sign_bit = nd500_is_negative(masked_result, fi->data_type);
    if (sign_bit) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }
}
