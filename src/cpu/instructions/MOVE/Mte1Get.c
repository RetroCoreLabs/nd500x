/*
 * Mte1Get.c - ND-500 Mte1Get instruction (MOVE class)
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
 * Mte1Get instruction - MOVE class
 *
 * Store MTE1 Register: MTE1 -> <dest>
 *
 * Variants: 1
 * Mnemonics: mte1=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFD70
 *
 * Operation: regs.MTE1 -> <dest>
 *
 * Description:
 *   Reads the MTE1 (Memory Table Entry 1) register and stores it to the
 *   destination operand as a word (32-bit). Updates Z and S flags based on
 *   the value.
 *
 *   The MTE registers contain memory table entry information used for
 *   memory management and address translation.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if MTE1 is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Mte1Get.cs
 */
void nd500_instr_Mte1Get(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] Mte1Get at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = cpu->MTE1;
    nd500_write_operand_word(cpu, &fi->operands[0], value);

    /* Z and S from the value; C and O are reset (manual 6.5.1; the microcode's
     * ST,SAVA status save, e.g. READ_RFEND @012243, STOREA1 @001170). */
    nd500_set_flags_zs(cpu, value, ND500_DTYPE_WORD);
}
