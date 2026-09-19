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
#include "float_math.h"

static uint64_t alog_base(uint64_t x, bool is_double, unsigned* exc) {
    return nd500_fm_alog(x, 2, is_double, exc);
}

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
    /* F and D are both computed the way the B30 microcode computes them
     * (float_math.h: same constants, reduction, polynomial and order of
     * AAP operations), not with the host maths library. */
    bool is_double = (fi->opcode >= 0xFFA8 && fi->opcode <= 0xFFAB);
    nd500_fm_unary(cpu, fi, "ALOG2", is_double, alog_base);
}
