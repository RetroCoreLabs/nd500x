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
    /* F and D: the true value of the function, rounded once to the format
     * by the 7.2.7 rule (ND-05.009.4 chapter 12, owner decision 2026-11-24).
     * float_math.c computes it in fixed point with an error bound and doubles
     * the precision until both ends of the bound round alike; no host maths
     * library. */
    bool is_double = (fi->opcode >= 0xFF84 && fi->opcode <= 0xFF87);
    nd500_fm_unary(cpu, fi, "SIN", is_double, nd500_fm_sin);
}
