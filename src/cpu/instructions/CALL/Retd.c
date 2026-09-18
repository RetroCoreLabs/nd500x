/*
 * Retd.c - ND-500 Retd instruction (CALL class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Retd instruction - CALL class
 *
 * Return Direct - Return from ENTD subroutine.
 * Simplest return instruction with no stack frame to unwind.
 *
 * Mnemonic: RETD
 * Operands: 0
 * Opcode: 0x0082
 *
 * Operation:
 *   PC <- L (restore program counter from link register)
 *
 * This instruction is paired with ENTD. Since ENTD doesn't create a stack
 * frame, RETD simply restores PC from the L register which contains the
 * return address.
 *
 * No stack frame operations are performed.
 * B register remains unchanged (still points to caller's frame).
 *
 * Reference: ND-500 Reference Manual and docs/instructions/asm/ (authoritative).
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Retd.cs
 */
void nd500_instr_Retd(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Simply restore PC from L register */
    cpu->PC = cpu->L;

    /* No stack frame to unwind */
    /* B register unchanged */
    /* L register unchanged (still contains return address) */
}
