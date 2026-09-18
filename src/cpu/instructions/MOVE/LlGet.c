/*
 * LlGet.c - ND-500 LlGet instruction (MOVE class)
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
 * LlGet instruction - MOVE class
 *
 * Store LL Register: LL -> <dest>
 *
 * Variants: 1
 * Mnemonics: ll=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFDC2
 *
 * Operation: regs.LL -> <dest>
 *
 * Description:
 *   Reads the LL (Low Level) register and stores it to the destination
 *   operand as a word (32-bit). Updates Z and S flags based on the value.
 *
 *   The LL register is used for stack frame management and points to the
 *   lowest level in the current stack frame.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if LL is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/LlGet.cs
 */
void nd500_instr_LlGet(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] LlGet at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = cpu->LL;
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
