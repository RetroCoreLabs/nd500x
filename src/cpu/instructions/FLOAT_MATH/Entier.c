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
#include "float_exact.h"

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

    uint64_t bits;
    bool is_double;
    if (fi->opcode == 0xFDC7) {
        bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT) & 0xFFFFFFFFull;
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        is_double = false;
    } else if (fi->opcode == 0xFDC8) {
        bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        is_double = true;
    } else {
        printf("[ERROR] ENTIER at PC=0x%08X: Unknown opcode 0x%04X\n",
               fi->address, fi->opcode);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* 10.37: "if int(source) > source then int(source) - 1 -> destination
     * else int(source) -> destination" - the floor of the source, worked out
     * on the exact value (7.2.5 / 7.2.6: +-significand * 2**scale). Until
     * 2026-11-24 this went through a host double, 53 bits against D's 55: a D
     * source just below a whole number (fraction only in the last two
     * mantissa bits) rounded up to it and the floor came out one high. Same as
     * RetroCore Entier.cs. Magnitudes of 2**32 and up are an overflow whatever
     * the sign, so they are not formed. */
    bool neg;
    uint64_t sig;
    int scale;
    nd500_fx_decode(bits, is_double, &neg, &sig, &scale);
    bool big = false;            /* |floor| >= 2**32 */
    uint64_t whole = 0;          /* the magnitude cut off: |int(source)| */
    bool fraction = false;
    if (sig == 0) {
        whole = 0;
    } else if (scale >= 0) {
        if (scale >= 32) {
            big = true;          /* sig >= 1, so the magnitude is >= 2**32 */
        } else {
            whole = sig << scale;   /* sig < 2**55, scale < 32: checked below */
            big = (scale > 0 && (sig >> (64 - scale)) != 0) || whole >= ((uint64_t)1 << 32);
        }
    } else if (-scale >= 64) {
        whole = 0;
        fraction = true;
    } else {
        whole = sig >> -scale;
        fraction = (sig & ((((uint64_t)1) << -scale) - 1)) != 0;
        big = whole >= ((uint64_t)1 << 32);
    }
    int64_t fl = 0;
    if (!big) {
        fl = neg ? -(int64_t)whole - (fraction ? 1 : 0) : (int64_t)whole;
    }
    int32_t result;
    bool overflow = false;
    if (big ? !neg : fl > INT32_MAX) {
        result = INT32_MAX;
        overflow = true;
    } else if (big ? neg : fl < INT32_MIN) {
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
