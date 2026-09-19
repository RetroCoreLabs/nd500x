/*
 * Asin.c - ND-500 Asin instruction (FLOAT_MATH class)
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

/**
 * Asin instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: asin asin
 * Operands: 1
 *
 * Opcodes:
 *   0xFF5C-0xFF5F (asin) - F1-F4 ASIN (float)
 *   0xFF88-0xFF8B (asin) - D1-D4 ASIN (double)
 *
 * Calculates arc sine: asin(argument) -> register
 * Result in radians (-pi/2 to pi/2).
 * Traps IVO if |argument| > 1.0.
 */
void nd500_instr_Asin(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* F and D are both computed the way the B30 microcode computes them
     * (float_math.h: same constants, reduction, polynomial and order of
     * AAP operations), not with the host maths library. */
    bool is_double = (fi->opcode >= 0xFF88 && fi->opcode <= 0xFF8B);
    nd500_fm_unary(cpu, fi, "ASIN", is_double, nd500_fm_asin);
}
