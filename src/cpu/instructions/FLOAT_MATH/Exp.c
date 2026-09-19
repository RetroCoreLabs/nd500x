/*
 * Exp.c - ND-500 Exp instruction (FLOAT_MATH class)
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
 * Exp instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: exp exp
 * Operands: 1
 *
 * Opcodes:
 *   0xFF74-0xFF77 (exp) - F1-F4 EXP (float)
 *   0xFFA0-0xFFA3 (exp) - D1-D4 EXP (double)
 *
 * Calculates exponential: e^argument -> register
 * Traps FO if result is infinity (overflow).
 */
void nd500_instr_Exp(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* F and D are both computed the way the B30 microcode computes them
     * (float_math.h: same constants, reduction, polynomial and order of
     * AAP operations), not with the host maths library. */
    bool is_double = (fi->opcode >= 0xFFA0 && fi->opcode <= 0xFFA3);
    nd500_fm_unary(cpu, fi, "EXP", is_double, nd500_fm_exp);
}
