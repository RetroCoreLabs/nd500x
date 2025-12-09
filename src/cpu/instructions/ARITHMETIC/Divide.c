#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * Divide instruction - ARITHMETIC class
 *
 * Divide Register by Operand: Rn / <operand> → Rn
 *
 * Variants: 5
 * Mnemonics: / (divide)
 * Operands: 1 (<operand/r/t>)
 *
 * Opcodes:
 *   0xFC4C (BYn /) byte divide
 *   0xFC50 (Hn /)  halfword divide
 *   0x0078 (Wn /)  word divide
 *   0x007C (Fn /)  float divide
 *   0x0080 (Dn /)  double divide
 *
 * Operation: Rn / <operand> → Rn
 *
 * Description:
 *   The contents of the specified register are divided by the operand.
 *   The quotient is stored in the register. The remainder is discarded.
 *   Division by zero causes a divide by zero trap.
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
 * Flags: Z (zero), S (sign), O (overflow), DZ (divide by zero)
 *   Z = 1 if quotient is zero
 *   S = 1 if sign bit is set
 *   O = 1 if overflow (MIN_VALUE / -1)
 *   DZ = 1 if divisor is zero
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Divide by zero (DZ)
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Basic Arithmetic)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Divide.cs
 */
void nd500_instr_Divide(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] Divide at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] Divide at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double variants */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        uint8_t reg_num = fi->target_register;

        /* Read register and operand */
        double reg_value = 0.0;
        double operand_value = 0.0;

        if (is_double) {
            uint64_t reg_bits = nd500_read_double_register(cpu, reg_num);
            reg_value = nd500_double_to_ieee754(reg_bits);
            uint64_t op_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
            operand_value = nd500_double_to_ieee754(op_bits);
        } else {
            uint32_t reg_bits = nd500_read_float_register(cpu, reg_num);
            reg_value = (double)nd500_float_to_ieee754(reg_bits);
            uint32_t op_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
            operand_value = (double)nd500_float_to_ieee754(op_bits);
        }

        /* Check for divide by zero */
        if (operand_value == 0.0) {
            cpu->ST1 |= ND500_FLAG_DZ;
            trap_divide_by_zero(cpu, fi->address);
            return;
        }
        cpu->ST1 &= ~ND500_FLAG_DZ;

        /* Perform division */
        double result = reg_value / operand_value;

        /* Check for overflow/underflow */
        if (isinf(result) || isnan(result)) {
            trap_floating_overflow(cpu, fi->address);
        } else if (result != 0.0 && fabs(result) < 1e-38) {
            /* Underflow - result too small */
            trap_floating_underflow(cpu, fi->address);
        }

        /* Convert result back to ND-500 format */
        uint64_t result_bits = 0;
        if (is_double) {
            result_bits = nd500_double_from_ieee754(result);
            nd500_write_double_register(cpu, reg_num, result_bits);
        } else {
            result_bits = nd500_float_from_ieee754((float)result);
            nd500_write_float_register(cpu, reg_num, (uint32_t)result_bits);
        }

        /* Update flags: Z (zero), S (sign) */
        if (is_double) {
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
            uint32_t float_bits = (uint32_t)result_bits;
            if (nd500_float_is_zero(float_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_Z);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_Z);
            }
            if (nd500_float_is_negative(float_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_S);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_S);
            }
        }

        /* C flag unaffected for float operations */
        /* O flag unaffected (overflow handled by FO trap) */
        return;
    }

    // Integer division
    uint32_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t divisor = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    // Check for divide by zero
    if (divisor == 0) {
        cpu->ST1 |= ND500_FLAG_DZ;
        trap_divide_by_zero(cpu, fi->address);
        return;
    }

    cpu->ST1 &= ~ND500_FLAG_DZ;

    // Perform signed division with overflow detection
    int64_t quotient = 0;
    bool overflow = false;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            int8_t dividend = (int8_t)reg_value;
            int8_t div = (int8_t)divisor;
            // Check for overflow: MIN_VALUE / -1
            if (dividend == INT8_MIN && div == -1) {
                overflow = true;
                quotient = dividend;  // Keep original value
            } else {
                quotient = dividend / div;
            }
            break;
        }
        case ND500_DTYPE_HALFWORD: {
            int16_t dividend = (int16_t)reg_value;
            int16_t div = (int16_t)divisor;
            // Check for overflow: MIN_VALUE / -1
            if (dividend == INT16_MIN && div == -1) {
                overflow = true;
                quotient = dividend;  // Keep original value
            } else {
                quotient = dividend / div;
            }
            break;
        }
        case ND500_DTYPE_WORD: {
            int32_t dividend = (int32_t)reg_value;
            int32_t div = (int32_t)divisor;
            // Check for overflow: MIN_VALUE / -1
            if (dividend == INT32_MIN && div == -1) {
                overflow = true;
                quotient = dividend;  // Keep original value
            } else {
                quotient = dividend / div;
            }
            break;
        }
        default:
            printf("[ERROR] Divide at PC=0x%08X: Invalid data type %u\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    // Mask result to data type
    uint32_t masked_result = nd500_mask_to_datatype(quotient, fi->data_type);

    // Write back to register
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    // Update status flags
    if (masked_result == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
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
