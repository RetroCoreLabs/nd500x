/*
 * Acos.c - ND-500 Acos instruction (FLOAT_MATH class)
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
 * Acos instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: acos acos
 * Operands: 1
 *
 * Opcodes:
 *   0xFF64-0xFF67 (acos) - F1-F4 ACOS (float)
 *   0xFF90-0xFF93 (acos) - D1-D4 ACOS (double)
 *
 * Calculates arc cosine: acos(argument) -> register
 * Result in radians (0 to pi).
 * Traps IVO if |argument| > 1.0.
 */
void nd500_instr_Acos(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* F and D are both computed the way the B30 microcode computes them
     * (float_math.h: same constants, reduction, polynomial and order of
     * AAP operations), not with the host maths library. */
    bool is_double = (fi->opcode >= 0xFF90 && fi->opcode <= 0xFF93);
    nd500_fm_unary(cpu, fi, "ACOS", is_double, nd500_fm_acos);
}
