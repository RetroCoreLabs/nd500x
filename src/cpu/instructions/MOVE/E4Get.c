/*
 * E4Get.c - ND-500 E4Get instruction (MOVE class)
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
 * E4Get instruction - MOVE class
 *
 * Store E4 Register (lower 32 bits): E4[31:0] -> <dest>
 *
 * Variants: 1
 * Mnemonics: e4=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFE3F
 *
 * Operation: regs.E4[31:0] -> <dest>
 *
 * Description:
 *   Reads the lower 32 bits of the E4 extended register and stores it to
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
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/E4Get.cs
 */
void nd500_instr_E4Get(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] E4Get at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // en=: reads the E4 register, the LEAST significant 32 bits of D4
    // (manual 16.9; microcode STOREE4 @001177: A,E4). An is the most
    // significant half (nd500_read_double_register).
    uint32_t value = cpu->E[3];

    nd500_write_operand_word(cpu, &fi->operands[0], value);

    /* Z and S from the value; C and O are reset (manual 6.5.1; the microcode's
     * ST,SAVA status save, e.g. READ_RFEND @012243, STOREA1 @001170). */
    nd500_set_flags_zs(cpu, value, ND500_DTYPE_WORD);
}
