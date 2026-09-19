/*
 * Tan.c - ND-500 Tan instruction (FLOAT_MATH class)
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
 * Tan instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: tan tan
 * Operands: 1
 *
 * Opcodes:
 *   0xFF68-0xFF6B (tan) - F1-F4 TAN (float)
 *   0xFF94-0xFF97 (tan) - D1-D4 TAN (double)
 *
 * Calculates tangent: tan(argument) -> register
 * Argument in radians.
 * Traps FO if result is infinity (near pi/2 + n*pi).
 */
void nd500_instr_Tan(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* F and D are both computed the way the B30 microcode computes them
     * (float_math.h: same constants, reduction, polynomial and order of
     * AAP operations), not with the host maths library. */
    bool is_double = (fi->opcode >= 0xFF94 && fi->opcode <= 0xFF97);
    nd500_fm_unary(cpu, fi, "TAN", is_double, nd500_fm_tan);
}
