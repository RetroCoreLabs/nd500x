/*
 * Ote1Get.c - ND-500 Ote1Get instruction (MOVE class)
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
 * Ote1Get instruction - MOVE class
 *
 * Store OTE1 Register: OTE1 -> <dest>
 *
 * Variants: 1
 * Mnemonics: ote1=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFDC5
 *
 * Operation: regs.OTE1 -> <dest>
 *
 * Description:
 *   Reads the OTE1 (Object Table Entry 1) register and stores it to the
 *   destination operand as a word (32-bit). Updates Z and S flags based on
 *   the value.
 *
 *   The OTE registers contain object table entry information used for
 *   object-oriented programming support and descriptor management.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if OTE1 is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Ote1Get.cs
 */
void nd500_instr_Ote1Get(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] Ote1Get at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = cpu->OTE1;
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
