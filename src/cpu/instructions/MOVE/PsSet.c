/*
 * PsSet.c - ND-500 PsSet instruction (MOVE class)
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
 * PsSet instruction - MOVE class
 *
 * Load PS Register: <source> -> PS
 *
 * Variants: 1
 * Mnemonics: ps:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFF44
 *
 * Operation: <source> -> regs.PS
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and stores it to the PS
 *   (Program Status) register. Updates Z and S flags based on the value.
 *
 *   The PS register contains program status information including interrupt
 *   level, privilege level, and other execution state flags. This instruction
 *   allows system software to restore or modify program status.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if value is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/PsSet.cs
 */
void nd500_instr_PsSet(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] PsSet at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_operand_word(cpu, &fi->operands[0]);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    cpu->PS = value;

    // Set Z and S flags based on the value
    if (value == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    if ((value & 0x80000000) != 0) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }
}
