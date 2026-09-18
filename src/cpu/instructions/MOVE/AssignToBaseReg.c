/*
 * AssignToBaseReg.c - ND-500 AssignToBaseReg instruction (MOVE class)
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

/**
 * AssignToBaseReg instruction - MOVE class
 *
 * Store Local Base Register: B -> <dest>
 *
 * Variants: 1
 * Mnemonics: b=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFC0A
 *
 * Operation: regs.B -> <dest>
 *
 * Description:
 *   Reads the B (local base) register and stores it to the destination operand
 *   as a word (32-bit). Updates Z and S flags based on the value.
 *
 *   The B register is used as a base pointer for local variable access in
 *   subroutines and provides support for structured programming.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if B is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10.5
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/AssignToBaseReg.cs
 */
void nd500_instr_AssignToBaseReg(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] AssignToBaseReg at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = cpu->B;
    nd500_write_operand_word(cpu, &fi->operands[0], value);

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
