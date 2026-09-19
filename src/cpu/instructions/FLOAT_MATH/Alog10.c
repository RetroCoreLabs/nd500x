/*
 * Alog10.c - ND-500 Alog10 instruction (FLOAT_MATH class)
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
    return nd500_fm_alog(x, 10, is_double, exc);
}

/**
 * Alog10 instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: alog10 alog10
 * Operands: 1
 *
 * Opcodes:
 *   0xFF80-0xFF83 (alog10) - F1-F4 ALOG10 (float)
 *   0xFFAC-0xFFAF (alog10) - D1-D4 ALOG10 (double)
 *
 * Calculates base-10 logarithm: log10(argument) -> register
 * Traps IVO if argument <= 0.
 */
void nd500_instr_Alog10(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* F and D are both computed the way the B30 microcode computes them
     * (float_math.h: same constants, reduction, polynomial and order of
     * AAP operations), not with the host maths library. */
    bool is_double = (fi->opcode >= 0xFFAC && fi->opcode <= 0xFFAF);
    nd500_fm_unary(cpu, fi, "ALOG10", is_double, alog_base);
}
