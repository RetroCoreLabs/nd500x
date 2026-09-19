/*
 * Atan.c - ND-500 Atan instruction (FLOAT_MATH class)
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

/* ATAN is ATAN2 with 1.0 as the second operand (ATANF @001400, ATAND). */
static uint64_t atan_one(uint64_t x, bool is_double, unsigned* exc) {
    return nd500_fm_atan2(x, nd500_fm_one(is_double), is_double, exc);
}

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
    /* F and D are both computed the way the B30 microcode computes them
     * (float_math.h: same constants, reduction, polynomial and order of
     * AAP operations), not with the host maths library. */
    bool is_double = (fi->opcode >= 0xFF98 && fi->opcode <= 0xFF9B);
    nd500_fm_unary(cpu, fi, "ATAN", is_double, atan_one);
}
