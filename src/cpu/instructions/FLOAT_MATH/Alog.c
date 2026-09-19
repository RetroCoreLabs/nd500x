/*
 * Alog.c - ND-500 Alog instruction (FLOAT_MATH class)
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
#include "float_math.h"

static uint64_t alog_base(uint64_t x, bool is_double, unsigned* exc) {
    return nd500_fm_alog(x, 0, is_double, exc);
}

/**
 * Alog instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: alog alog
 * Operands: 1
 *
 * Opcodes:
 *   0xFF78-0xFF7B (alog) - F1-F4 ALOG (float)
 *   0xFFA4-0xFFA7 (alog) - D1-D4 ALOG (double)
 *
 * Calculates natural logarithm: ln(argument) -> register
 * Traps IVO if argument <= 0.
 */
void nd500_instr_Alog(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* F and D are both computed the way the B30 microcode computes them
     * (float_math.h: same constants, reduction, polynomial and order of
     * AAP operations), not with the host maths library. */
    bool is_double = (fi->opcode >= 0xFFA4 && fi->opcode <= 0xFFA7);
    nd500_fm_unary(cpu, fi, "ALOG", is_double, alog_base);
}
