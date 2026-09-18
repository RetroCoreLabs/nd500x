/*
 * PGet.c - ND-500 PGet instruction (MOVE class)
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
 * PGet instruction - MOVE class
 *
 * Store PC Register: PC -> <dest>
 *
 * Variants: 1
 * Mnemonics: p=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFD62
 *
 * Operation: regs.PC -> <dest>
 *
 * Description:
 *   Reads the PC (Program Counter) register and stores it to the destination
 *   operand as a word (32-bit). Updates Z and S flags based on the value.
 *
 *   The PC register contains the address of the next instruction to execute.
 *   This instruction allows programs to capture the current instruction pointer
 *   for position-independent code or self-modifying code scenarios.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if PC is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/PGet.cs
 */
void nd500_instr_PGet(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] PGet at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* "When storing the program counter ( P=: ), the contents of the operand
     * will be the address of the P=: instruction" (manual 16.8). The B30
     * microcode reads IAC,NPC (STORP @001141), which "points to beginning of
     * an instruction until and including a fetch operation" (ND-05.022.1).
     * cpu->PC has already moved past this instruction. */
    uint32_t value = fi->address;
    nd500_write_operand_word(cpu, &fi->operands[0], value);

    /* Z and S from the value; C and O are reset (manual 6.5.1; the microcode's
     * ST,SAVA status save, e.g. READ_RFEND @012243, STOREA1 @001170). */
    nd500_set_flags_zs(cpu, value, ND500_DTYPE_WORD);
}
