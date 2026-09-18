/*
 * CedGet.c - ND-500 CedGet instruction (MOVE class)
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
 * CedGet instruction - MOVE class
 *
 * Store CED Register: CED -> <dest>
 *
 * Variants: 1
 * Mnemonics: ced=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFE54
 *
 * Operation: regs.CED -> <dest>
 *
 * Description:
 *   Reads the CED (Current Environment Descriptor) register and stores it to
 *   the destination operand as a word (32-bit). Updates Z and S flags based
 *   on the value.
 *
 *   The CED register contains the current environment descriptor used for
 *   process/thread context management.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if CED is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/CedGet.cs
 */
void nd500_instr_CedGet(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] CedGet at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = cpu->CED;
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
