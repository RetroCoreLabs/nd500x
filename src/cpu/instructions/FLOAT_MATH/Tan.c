#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * Tan instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: tan tan
 * Operands: 1
 *
 * Opcodes:
 *   0xFF68-0xFF6B (tan) - F1-F4 TAN (float)
 *   0xFF94-0xFF97 (tan) - D1-D4 TAN (double)
 *
 * Calculates tangent: tan(argument) -> register
 * Argument in radians.
 * Traps FO if result is infinity (near pi/2 + n*pi).
 */
void nd500_instr_Tan(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] TAN expects 1 operand, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine if float or double from opcode */
    bool is_double = (fi->opcode >= 0xFF94 && fi->opcode <= 0xFF97);
    uint8_t reg_num = fi->target_register;

    if (reg_num < 1 || reg_num > 4) {
        printf("[ERROR] TAN at PC=0x%08X: Invalid register %u\n",
               fi->address, reg_num);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    double argument = 0.0;
    double result = 0.0;
    uint64_t result_bits = 0;

    /* Read argument operand */
    if (is_double) {
        uint64_t arg_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        argument = nd500_double_to_ieee754(arg_bits);
    } else {
        uint32_t arg_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT);
        argument = (double)nd500_float_to_ieee754(arg_bits);
    }

    /* Calculate tangent */
    result = tan(argument);

    /* Check for overflow/invalid (infinity or NaN) */
    if (isinf(result)) {
        trap_floating_overflow(cpu, fi->address);
        result = 0.0;
    } else if (isnan(result)) {
        trap_invalid_operation(cpu, fi->address);
        result = 0.0;
    }

    /* Convert result back to ND-500 format */
    if (is_double) {
        result_bits = nd500_double_from_ieee754(result);
    } else {
        result_bits = nd500_float_from_ieee754((float)result);
    }

    /* Write result to register */
    if (is_double) {
        nd500_write_double_register(cpu, reg_num, result_bits);
    } else {
        nd500_write_float_register(cpu, reg_num, (uint32_t)result_bits);
    }

    /* Set flags: Z (zero), S (sign) */
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

    /* O, C flags unaffected */
}
