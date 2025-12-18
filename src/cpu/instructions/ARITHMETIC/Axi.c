#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * AXI instruction - ARITHMETIC class
 *
 * AXI - A to the I'th Power (Floating Point Exponentiation)
 *
 * Format: tn AXI <a/r/t>, <i/r/W>
 *
 * Assembly:
 *   Fn AXI (float A to the I'th power)         Hex 0xFCC0 + (n-1)
 *   Dn AXI (double float A to the I'th power)  Hex 0xFCC4 + (n-1)
 *
 * Operation: <a>**<i> -> Rn
 *
 * Description:
 *   The floating-point operand <a> is raised to the power of the integer
 *   operand <i> and the result is stored in the specified floating-point
 *   register. The exponent <i> must be an integer value.
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *   - Invalid operation (IVO)
 *
 * Data status bits:
 *   - result = 0 -> Z
 *   - result.signbit -> S
 *   - floating underflow -> FU
 *   - floating overflow -> FO
 *   - invalid operation -> IVO
 *
 * Reference: ND-500 Reference Manual, Chapter 12.1 (Mathematical Functions)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Axi.cs
 */
void nd500_instr_Axi(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] AXI at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check that we're using float registers */
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] AXI at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read floating-point operand a and integer operand i */
    uint64_t float_bits = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    int32_t exponent_i = (int32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);

    double result = 0.0;
    bool overflow = false;
    bool underflow = false;
    bool invalid_op = false;

    /* Convert to IEEE 754 for calculation */
    double base_value;
    if (fi->data_type == ND500_DTYPE_FLOAT) {
        base_value = nd500_float_to_ieee754((uint32_t)float_bits);
    } else {
        base_value = nd500_double_to_ieee754(float_bits);
    }

    /* Handle special cases */
    if (exponent_i == 0) {
        /* a^0 = 1 (for any a != 0) */
        result = 1.0;
    } else if (exponent_i < 0) {
        /* Negative exponent: a^(-i) = 1 / (a^i) */
        if (base_value == 0.0) {
            /* 0^(-n) is invalid (division by zero) */
            invalid_op = true;
            result = 0.0;
        } else {
            result = pow(base_value, (double)exponent_i);
            if (isinf(result)) {
                overflow = true;
            } else if (result != 0.0 && fabs(result) < 1e-308) {
                underflow = true;
            }
        }
    } else {
        /* Positive exponent */
        if (base_value == 0.0) {
            /* 0^n = 0 (for n > 0) */
            result = 0.0;
        } else if (base_value == 1.0) {
            /* 1^n = 1 */
            result = 1.0;
        } else {
            /* General case using pow() */
            result = pow(base_value, (double)exponent_i);
            if (isinf(result)) {
                overflow = true;
            } else if (result != 0.0 && fabs(result) < 1e-308) {
                underflow = true;
            }
        }
    }

    /* Convert result back to ND-500 format and store */
    uint64_t result_bits;
    if (fi->data_type == ND500_DTYPE_FLOAT) {
        result_bits = nd500_float_from_ieee754((float)result);
        nd500_write_float_register(cpu, fi->target_register, (uint32_t)result_bits);
    } else {
        result_bits = nd500_double_from_ieee754(result);
        nd500_write_double_register(cpu, fi->target_register, result_bits);
    }

    /* Update status flags */
    if (result == 0.0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (result < 0.0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* Set floating-point exception flags */
    if (overflow) {
        cpu->ST1 |= ND500_FLAG_FO;
    }
    if (underflow) {
        cpu->ST1 |= ND500_FLAG_FU;
    }
    if (invalid_op) {
        /* Use K flag for invalid operations */
        cpu->ST1 |= ND500_FLAG_K;
    }
}
