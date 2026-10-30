/*
 * Entier.c - ND-500 Entier instruction (FLOAT_MATH class)
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
#include <math.h>

/**
 * Entier instruction - FLOAT_MATH class, '87 extension
 *
 * ENTIER - SIMULA Entier function
 *
 * Format: t ENTIER <source/r/t1>,<destination/w/W>
 *
 * Opcodes (ND-05.009.4 section 10.37, octal column):
 *   0xFDC7 (176707) - F ENTIER
 *   0xFDC8 (176710) - D ENTIER
 *
 * Operation: the largest integer not greater than the source (floor),
 * stored as a 32 bit integer.
 *   if int(source) > source then int(source) - 1 -> destination
 *   else int(source) -> destination
 *
 * Data status bits: result = 0 -> Z, sign of result -> S, integer overflow -> O.
 *
 * What is settled (manual and the B30 microcode listing agree): floor, a 32 bit
 * result, O from the conversion, and no trap on overflow.
 *
 * What is NOT settled and is marked here on purpose:
 *   - The value written when the floor is outside the 32 bit range. This file
 *     clamps to the largest or smallest 32 bit integer, as WCONV does here.
 *   - What Z and S show after an overflow, and the result for a NaN-like source.
 */
void nd500_instr_Entier(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] ENTIER at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    double value;
    if (fi->opcode == 0xFDC7) {
        uint32_t bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT);
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        value = nd500_native_single_to_double(bits);
    } else if (fi->opcode == 0xFDC8) {
        uint64_t bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        value = nd500_native_double_to_double(bits);
    } else {
        printf("[ERROR] ENTIER at PC=0x%08X: Unknown opcode 0x%04X\n",
               fi->address, fi->opcode);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    double fl = floor(value);
    int32_t result;
    bool overflow = false;
    if (fl >= 2147483648.0) {
        result = INT32_MAX;
        overflow = true;
    } else if (fl < -2147483648.0) {
        result = INT32_MIN;
        overflow = true;
    } else {
        result = (int32_t)fl;
    }

    nd500_write_operand_value(cpu, &fi->operands[1], (uint32_t)result, ND500_DTYPE_WORD);
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    nd500_set_flags_zs(cpu, (uint32_t)result, ND500_DTYPE_WORD);
    if (overflow) {
        nd500_set_flag(cpu, ND500_FLAG_O);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_O);
    }
    nd500_clear_flag(cpu, ND500_FLAG_C);
}
