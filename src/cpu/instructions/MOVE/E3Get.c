/*
 * E3Get.c - ND-500 E3Get instruction (MOVE class)
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
 * E3Get instruction - MOVE class
 *
 * Store E3 Register (lower 32 bits): E3[31:0] -> <dest>
 *
 * Variants: 1
 * Mnemonics: e3=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFE3E
 *
 * Operation: regs.E3[31:0] -> <dest>
 *
 * Description:
 *   Reads the lower 32 bits of the E3 extended register and stores it to
 *   the destination operand as a word (32-bit). Updates Z and S flags
 *   based on the word value.
 *
 *   Note: E-registers are 64-bit but this instruction only transfers the
 *   lower 32 bits. Upper 32 bits remain unchanged in the register.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if lower 32 bits are zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/E3Get.cs
 */
void nd500_instr_E3Get(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] E3Get at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // en=: reads the E3 register (E = HIGH 32 bits of D3;
    // microcode STOREE3 @001176: A,E3). Was reading the low half (A3).
    uint32_t value = cpu->E[2];

    nd500_write_operand_word(cpu, &fi->operands[0], value);

    // Set Z and S flags based on the 32-bit word value
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

    // ST,SAVA: C and O cleared; K unchanged.
    cpu->ST1 &= ~(ND500_FLAG_C | ND500_FLAG_O);
}
