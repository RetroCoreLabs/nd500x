/*
 * Alog2.c - ND-500 Alog2 instruction (FLOAT_MATH class)
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
 * Alog2 instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: alog2 (F1-F4, D1-D4)
 * Operands: 1
 *
 * Opcodes:
 *   0xFF7C-0xFF7F (alog2) - F1-F4 ALOG2 (float)
 *   0xFFA8-0xFFAB (alog2) - D1-D4 ALOG2 (double)
 *
 * Calculates base-2 logarithm: log2(argument) -> register
 */
void nd500_instr_Alog2(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] ALOG2 expects 1 operand, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine if float or double from opcode */
    bool is_double = (fi->opcode >= 0xFFA8 && fi->opcode <= 0xFFAB);
    uint8_t reg_num = fi->target_register;

    if (reg_num < 1 || reg_num > 4) {
        printf("[ERROR] ALOG2 at PC=0x%08X: Invalid register %u\n",
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
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        argument = nd500_double_to_ieee754(arg_bits);
    } else {
        uint32_t arg_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        argument = (double)nd500_float_to_ieee754(arg_bits);
    }

    /* Check for invalid operation (argument <= 0) */
    if (argument <= 0.0) {
        /* Set result to -5.8x10^76 (largest negative float) */
        if (is_double) {
            result_bits = 0xFFE0000000000000ull;  /* Maximum negative double */
        } else {
            result_bits = 0xFFE00000u;  /* Maximum negative float */
        }
        trap_invalid_operation(cpu, fi->address);
        /* Continue to write result even on trap */
    } else {
        /* Calculate log2(argument) */
        result = log2(argument);

        /* Convert result back to ND-500 format */
        if (is_double) {
            result_bits = nd500_double_from_ieee754(result);
        } else {
            result_bits = nd500_float_from_ieee754((float)result);
        }
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
