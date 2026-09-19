/*
 * Cos.c - ND-500 Cos instruction (FLOAT_MATH class)
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
 * Cos instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: cos cos
 * Operands: 1
 *
 * Opcodes:
 *   0xFF60-0xFF63 (cos) - F1-F4 COS (float)
 *   0xFF8C-0xFF8F (cos) - D1-D4 COS (double)
 *
 * Calculates cosine: cos(argument) -> register
 * Argument in radians, max |arg| = 65536.
 * Traps IVO if |argument| > 65536.
 */
void nd500_instr_Cos(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* F and D are both computed the way the B30 microcode computes them
     * (float_math.h: same constants, reduction, polynomial and order of
     * AAP operations), not with the host maths library. */
    bool is_double = (fi->opcode >= 0xFF8C && fi->opcode <= 0xFF8F);
    nd500_fm_unary(cpu, fi, "COS", is_double, nd500_fm_cos);
}
