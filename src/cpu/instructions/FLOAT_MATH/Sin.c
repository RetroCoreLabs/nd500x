/*
 * Sin.c - ND-500 Sin instruction (FLOAT_MATH class)
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
 * Sin instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: sin sin
 * Operands: 1
 *
 * Opcodes:
 *   0xFF58-0xFF5B (sin) - F1-F4 SIN (float)
 *   0xFF84-0xFF87 (sin) - D1-D4 SIN (double)
 *
 * Calculates sine: sin(argument) -> register
 * Argument in radians, max |arg| = 65536.
 * Traps IVO if |argument| > 65536.
 */
void nd500_instr_Sin(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* F and D are both computed the way the B30 microcode computes them
     * (float_math.h: same constants, reduction, polynomial and order of
     * AAP operations), not with the host maths library. */
    bool is_double = (fi->opcode >= 0xFF84 && fi->opcode <= 0xFF87);
    nd500_fm_unary(cpu, fi, "SIN", is_double, nd500_fm_sin);
}
