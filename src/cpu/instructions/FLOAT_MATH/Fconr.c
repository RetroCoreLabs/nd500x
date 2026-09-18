/*
 * Fconr.c - ND-500 Fconr instruction (FLOAT_MATH class)
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
#include <stdint.h>

/**
 * Fconr instruction - FLOAT_MATH class
 *
 * Variants: 2
 * Mnemonics: fconr fconr
 * Operands: 2
 *
 * Opcodes:
 *   0xFE83 (fconr) - W FCONR (word to float with rounding)
 *   0xFE84 (fconr) - D FCONR (double to float with rounding)
 *
 * Converts word/double source to float with rounding.
 */
void nd500_instr_Fconr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] FCONR expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine source type from opcode */
    bool is_double_source = (fi->opcode == 0xFE84);
    uint32_t float_result = 0;

    /* Read and convert source operand */
    if (is_double_source) {
        /* D FCONR: Double to float */
        uint64_t double_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        double ieee_val = nd500_double_to_ieee754(double_bits);
        float_result = nd500_float_from_ieee754((float)ieee_val);
    } else {
        /* W FCONR: Word to float */
        int32_t word_val = (int32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        float_result = nd500_float_from_int32(word_val);
    }

    /* Write result to destination operand as FLOAT so a register
     * destination goes to A1-A4, not I1-I4 */
    nd500_write_operand_value(cpu, &fi->operands[1], (uint64_t)float_result, ND500_DTYPE_FLOAT);

    /* Set flags: Z (zero), S (sign) */
    if (nd500_float_is_zero(float_result)) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (nd500_float_is_negative(float_result)) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* O and C flags cleared */
    nd500_clear_flag(cpu, ND500_FLAG_O);
    nd500_clear_flag(cpu, ND500_FLAG_C);
}
