/*
 * Intr.c - ND-500 Intr instruction (SYSTEM class)
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
#include <stdio.h>
#include <stdint.h>

/**
 * Intr instruction - SYSTEM class
 *
 * INTR - Integer part WITH ROUNDING (manual ch.10.35)
 *
 * Format: tn INTR <x/r/t>
 *
 * | Assembly | Name                                  | Hex            | Octal          |
 * |----------|---------------------------------------|----------------|----------------|
 * | Fn INTR  | float integer part with rounding      | 0FE68H+(n-1)   | 177150B+(n-1)  |
 * | Dn INTR  | double float integer part with rounding | 0FE6CH+(n-1) | 177154B+(n-1)  |
 *
 * Operation: rounded integer part of <x> in FLOAT format -> Rn
 *
 * Trap conditions: Addressing traps. NOT privileged.
 *
 * Data status bits:
 *   result = 0        -> Z
 *   result.signbit    -> S
 *   (C and O are not named, so they are reset - manual 6.5.1)
 *
 * This is the rounding sibling of INT (ch.10.34, 0xFE60-0xFE67), which
 * truncates. The two differ only in that step.
 *
 * ---------------------------------------------------------------------------
 * REWRITTEN 2026-08-09 - this was the WRONG INSTRUCTION ENTIRELY.
 *
 * It used to implement "read the interrupt request register", return a
 * hardcoded 0, and require privilege. Nothing about that is INTR. The old
 * header even documented the operands as "Hn INTR"/"Wn INTR" (halfword/word)
 * when the instruction is float/double.
 *
 * Three independent sources say so, and they agree:
 *   1. Manual ch.10.35 - the entry quoted above.
 *   2. The dispatch table in this repo, src/cpu/nd500_instructions.c:342-349,
 *      maps 0xFE68-0xFE6B and 0xFE6C-0xFE6F, which is exactly the manual's
 *      Fn/Dn INTR opcode range.
 *   3. The ND-5000 control store. Its label file has INTRF (002613) and INTRD
 *      (002615) as the float and double routines, and RetroCore's dispatch map
 *      generated from that same .LABE reads
 *      "map[65128] = new DispatchEntry(1419, 0, 1); // F1 INTR" - 65128 = 0xFE68.
 *
 * Consequence of the old code: any unprivileged program using Fn INTR took an
 * IIC trap instead of getting a rounded float, and any privileged one silently
 * got 0. There was no corpus coverage - a search of all 40088 cases found zero
 * INTR tests - which is why this survived. Corpus cases are specified in
 * docs/SPEC-CORPUS-SOLO-TUTTI-DT-DE.md's companion entry for INTR.
 *
 * ROUNDING MODE - INFERRED, NOT PROVEN.
 * The manual says only "The result is rounded." round() below is
 * round-half-away-from-zero. The alternative worth considering is
 * round-half-to-even. The microcode distinguishes the two instructions exactly
 * at the step where they must differ:
 *     020335 INTF_1:  ALU,FZRO A,BM00 B,X1 D,SC6   (truncating sibling)
 *     020345 INTRF_1: ALU,A-1  A,BM26 B,X1 D,SC2   (this instruction)
 * BM26 is octal, so it is bit 22; A-1 makes a 22-bit mask. Tracing INTRF_0
 * (020341) and INTRD_0 (020361) to their returns would settle the mode. That
 * was not done here, so any test asserting the .5 cases must treat the
 * half-way behaviour as unverified rather than baking this choice in.
 * ---------------------------------------------------------------------------
 */
void nd500_instr_Intr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* 0xFE6C-0xFE6F are the Dn forms, the four below them the Fn forms. The F
     * operand is read as FLOAT: read as WORD, a register operand came from the
     * I bank, so F1 INTR A2 used I2. The work is shared with INT
     * and done exactly on the ND-500 bits (instruction_helpers.c). */
    bool is_double = (fi->opcode >= 0xFE6C && fi->opcode <= 0xFE6F);
    nd500_execute_integer_part(cpu, fi, is_double, true);
}
