#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * Sub instruction - ARITHMETIC class
 *
 * Subtract operand from register. Rn = Rn - operand
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn-, Hn-, Wn-, Fn-, Dn- (n=1..4)
 * Operands: 1 (value to subtract)
 *
 * Opcodes:
 *   0xFC3C-0xFC3F (BY1- through BY4-) - Byte subtract
 *   0xFC40-0xFC43 (H1- through H4-) - Halfword subtract
 *   0x0060-0x0063 (W1- through W4-) - Word subtract
 *   0x0064-0x0067 (F1- through F4-) - Float subtract
 *   0x0068-0x006B (D1- through D4-) - Double subtract
 *
 * Operation: Rn <- Rn - operand
 *
 * Flags: Z (zero), S (sign), C (carry/borrow), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if NO borrow (minuend >= subtrahend)
 *   O = 1 if signed overflow occurred
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Subtract.cs
 */
void nd500_instr_Sub(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] SUB at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double variants */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        uint8_t reg_num = fi->target_register;

        if (reg_num < 1 || reg_num > 4) {
            printf("[ERROR] SUB at PC=0x%08X: Invalid register %u\n",
                   fi->address, reg_num);
            trap_illegal_operand(cpu, fi->address);
            return;
        }

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

        /* Perform subtraction */
        double result = reg_value - operand_value;

        /* Check for overflow/underflow */
        if (isinf(result) || isnan(result)) {
            trap_floating_overflow(cpu, fi->address);
        } else if (result != 0.0 && fabs(result) < 1e-38) {
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
        return;
    }

    /* Read register and operand (like C# ReadIntegerRegister + ReadOperandValue) */
    uint64_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t operand = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Perform subtraction */
    uint64_t result = reg_value - operand;

    /* ND-500 carry convention: C=1 means NO borrow (minuend >= subtrahend)
     * Reference: ND-500 Reference Manual Page 2040, SUBC formula Page 194 */
    bool carry = (reg_value >= operand);
    bool overflow = nd500_detect_sub_overflow(reg_value, operand, result, fi->data_type);

    /* Mask to data type (like C# MaskToDataType) */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to register */
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    /* Update status flags: Z, S, C, O */
    nd500_set_flags_zsco(cpu, masked_result, fi->data_type, carry, overflow);
}
