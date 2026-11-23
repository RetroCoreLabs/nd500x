/*
 * Hconr.c - ND-500 Hconr instruction (FLOAT_MATH class)
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
 * Hconr instruction - FLOAT_MATH class
 *
 * Variants: 2
 * Mnemonics: hconr hconr
 * Operands: 2
 *
 * Opcodes:
 *   0xFE72 (hconr) - F HCONR (float to halfword with rounding)
 *   0xFE73 (hconr) - D HCONR (double to halfword with rounding)
 *
 * Converts float/double source to halfword with rounding (rounded, 15.3 and 7.2.7).
 * Traps IOV if value outside halfword range (-32768 to 32767).
 */
void nd500_instr_Hconr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] HCONR expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine source type from opcode */
    bool is_double = (fi->opcode == 0xFE73);
    int64_t source_value = 0;
    int16_t halfword_result = 0;
    bool overflow = false;

    /* Read and convert source operand */
    if (is_double) {
        /* D HCONR: Double to halfword (rounded, 15.3 and 7.2.7) */
        uint64_t double_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        /* 15.3 "The result is rounded": 7.2.7 at the integer position, a half to the even neighbour. */
        bool too_big = false;
        source_value = nd500_round_to_integer(double_bits, true, &too_big);
        if (too_big) overflow = true;
        if (source_value < -32768 || source_value > 32767) {
            overflow = true;
        }
    } else {
        /* F HCONR: Float to halfword (rounded, 15.3 and 7.2.7).
         * Read as FLOAT so a register operand comes from A1-A4, not I1-I4. */
        uint32_t float_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        bool too_big = false;
        source_value = nd500_round_to_integer(float_bits, false, &too_big);
        if (too_big || source_value < -32768 || source_value > 32767) overflow = true;
    }

    /* Check for overflow trap */
    if (overflow) {
        ND500X_TRAPLOG("[TRAP] HCONR at PC=0x%08X: Value %lld outside halfword range (-32768 to 32767)\n",
               fi->address, (long long)source_value);
        raise_trap(cpu, TRAP_O, fi->address, 0);
        return;
    }

    /* Convert to halfword (truncate) */
    halfword_result = (int16_t)source_value;

    /* Write result to destination operand */
    nd500_write_operand_value(cpu, &fi->operands[1], (uint64_t)(uint16_t)halfword_result, ND500_DTYPE_HALFWORD);

    /* Set flags: Z (zero), S (sign) */
    if (halfword_result == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (halfword_result < 0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* O and C flags cleared */
    nd500_clear_flag(cpu, ND500_FLAG_O);
    nd500_clear_flag(cpu, ND500_FLAG_C);
}
