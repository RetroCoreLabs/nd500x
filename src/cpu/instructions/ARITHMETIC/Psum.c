/*
 * Psum.c - ND-500 PSUM instruction (ARITHMETIC class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * PSUM instruction - ARITHMETIC class
 *
 * Sum of products (multiply-accumulate)
 *
 * Format: tn PSUM <x/r/t>, <y/r/t>
 *
 * Operation:
 *   <x> * <y> + Rn -> Rn
 *
 * The <x> operand is multiplied by the <y> operand and the product
 * is added to the contents of the specified register.
 *
 * Opcodes:
 *   0xFCF8-0xFCFB: BYn PSUM (byte, n=1-4)
 *   0xFCFC-0xFCFF: Hn PSUM  (halfword, n=1-4)
 *   0xFD00-0xFD03: Wn PSUM  (word, n=1-4)
 *   0xFD04-0xFD07: Fn PSUM  (float, n=1-4)
 *   0xFD08-0xFD0B: Dn PSUM  (double, n=1-4)
 *
 * Target register determined by (opcode & 0x03) + 1:
 *   0 -> register 1 (I1/F1/D1)
 *   1 -> register 2 (I2/F2/D2)
 *   2 -> register 3 (I3/F3/D3)
 *   3 -> register 4 (I4/F4/D4)
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Data status bits:
 *   result = 0         -> Z
 *   result.signbit     -> S
 *   carry from MSB     -> C (integer only)
 *   overflow           -> O (integer only)
 *   floating underflow -> FU (float/double only)
 *   floating overflow  -> FO (float/double only)
 *
 * Reference: ND-500 Reference Manual, Page 182
 */
void nd500_instr_Psum(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] PSUM at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get target register number (1-4) */
    uint8_t reg_num = fi->target_register;

    /* Check if this is a floating-point variant */
    if (fi->uses_float_registers) {
        /* Floating-point PSUM: x * y + Rn -> Rn */
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);

        /* Read operands */
        uint64_t x_bits = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
        uint64_t y_bits = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        /* Read current register value */
        uint64_t reg_bits;
        if (is_double) {
            reg_bits = nd500_read_double_register(cpu, reg_num);
        } else {
            reg_bits = nd500_read_float_register(cpu, reg_num);
        }

        /* Convert to IEEE 754 for calculation */
        double fp_x, fp_y, fp_reg;
        if (is_double) {
            fp_x = nd500_double_to_ieee754(x_bits);
            fp_y = nd500_double_to_ieee754(y_bits);
            fp_reg = nd500_double_to_ieee754(reg_bits);
        } else {
            fp_x = (double)nd500_float_to_ieee754((uint32_t)x_bits);
            fp_y = (double)nd500_float_to_ieee754((uint32_t)y_bits);
            fp_reg = (double)nd500_float_to_ieee754((uint32_t)reg_bits);
        }

        /* Calculate: x * y + reg */
        double product = fp_x * fp_y;
        double fp_result = product + fp_reg;

        /* Check for floating-point exceptions */
        bool fp_overflow = false;
        bool fp_underflow = false;

        if (isinf(fp_result)) {
            fp_overflow = true;
        } else if (fp_result != 0.0 && fabs(fp_result) < 2.2250738585072014e-308) {
            fp_underflow = true;
        }

        /* Convert result back to ND500 format and write to register */
        if (is_double) {
            uint64_t result_bits = nd500_double_from_ieee754(fp_result);
            nd500_write_double_register(cpu, reg_num, result_bits);
        } else {
            uint32_t result_bits = nd500_float_from_ieee754((float)fp_result);
            nd500_write_float_register(cpu, reg_num, result_bits);
        }

        /* Update status flags */
        nd500_clear_flag(cpu, ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O |
                              ND500_FLAG_FO | ND500_FLAG_FU);

        if (fp_result == 0.0) {
            nd500_set_flag(cpu, ND500_FLAG_Z);
        }
        if (fp_result < 0.0) {
            nd500_set_flag(cpu, ND500_FLAG_S);
        }
        if (fp_overflow) {
            nd500_set_flag(cpu, ND500_FLAG_FO);
        }
        if (fp_underflow) {
            nd500_set_flag(cpu, ND500_FLAG_FU);
        }

        return;
    }

    /* Integer PSUM: x * y + Rn -> Rn */

    /* Read operands */
    uint64_t x_raw = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    uint64_t y_raw = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Read current register value */
    uint32_t reg_raw = nd500_read_integer_register(cpu, reg_num);

    /* Sign-extend values based on data type for calculation */
    int64_t x_val, y_val, reg_val;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            x_val = (int8_t)(x_raw & 0xFF);
            y_val = (int8_t)(y_raw & 0xFF);
            reg_val = (int8_t)(reg_raw & 0xFF);
            break;
        case ND500_DTYPE_HALFWORD:
            x_val = (int16_t)(x_raw & 0xFFFF);
            y_val = (int16_t)(y_raw & 0xFFFF);
            reg_val = (int16_t)(reg_raw & 0xFFFF);
            break;
        case ND500_DTYPE_WORD:
        default:
            x_val = (int32_t)x_raw;
            y_val = (int32_t)y_raw;
            reg_val = (int32_t)reg_raw;
            break;
    }

    /* Calculate: x * y + reg */
    int64_t product = x_val * y_val;
    int64_t result = product + reg_val;

    /* Check for overflow based on data type */
    bool overflow = false;
    bool carry = false;
    uint32_t masked_result;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            overflow = (result < -128 || result > 127);
            carry = ((uint64_t)result > 0xFF);
            masked_result = (uint32_t)(result & 0xFF);
            break;
        case ND500_DTYPE_HALFWORD:
            overflow = (result < -32768 || result > 32767);
            carry = ((uint64_t)result > 0xFFFF);
            masked_result = (uint32_t)(result & 0xFFFF);
            break;
        case ND500_DTYPE_WORD:
        default:
            overflow = (result < INT32_MIN || result > INT32_MAX);
            carry = ((uint64_t)result > 0xFFFFFFFF);
            masked_result = (uint32_t)result;
            break;
    }

    /* Write result to register */
    nd500_write_integer_register(cpu, reg_num, masked_result);

    /* Update status flags */
    nd500_clear_flag(cpu, ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O);

    if (masked_result == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    }

    /* Check sign bit based on data type */
    uint32_t sign_mask;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            sign_mask = 0x80;
            break;
        case ND500_DTYPE_HALFWORD:
            sign_mask = 0x8000;
            break;
        case ND500_DTYPE_WORD:
        default:
            sign_mask = 0x80000000;
            break;
    }

    if (masked_result & sign_mask) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    }
    if (carry) {
        nd500_set_flag(cpu, ND500_FLAG_C);
    }
    if (overflow) {
        nd500_set_flag(cpu, ND500_FLAG_O);
    }
}
