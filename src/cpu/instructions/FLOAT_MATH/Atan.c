/*
 * Atan.c - ND-500 Atan instruction (FLOAT_MATH class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * Atan instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: atan atan
 * Operands: 1
 *
 * Opcodes:
 *   0xFF6C-0xFF6F (atan) - F1-F4 ATAN (float)
 *   0xFF98-0xFF9B (atan) - D1-D4 ATAN (double)
 *
 * Calculates arc tangent: atan(argument) -> register
 * Result in radians (-pi/2 to pi/2).
 * No domain restrictions - accepts all real numbers.
 */
void nd500_instr_Atan(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] ATAN expects 1 operand, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine if float or double from opcode */
    bool is_double = (fi->opcode >= 0xFF98 && fi->opcode <= 0xFF9B);
    uint8_t reg_num = fi->target_register;

    if (reg_num < 1 || reg_num > 4) {
        printf("[ERROR] ATAN at PC=0x%08X: Invalid register %u\n",
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

    /* Calculate arc tangent - no domain restrictions */
    result = atan(argument);

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
