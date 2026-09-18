/*
 * St1Get.c - ND-500 St1Get instruction (MOVE class)
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
 * St1Get instruction - MOVE class
 *
 * Store ST1 Register: ST1 -> <dest>
 *
 * Variants: 1
 * Mnemonics: st1=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFDC3
 *
 * Operation: regs.ST.ST1 -> <dest>
 *
 * Description:
 *   Reads the ST1 (Status Register 1) and stores it to the destination
 *   operand as a word (32-bit). Updates Z and S flags based on the value.
 *
 *   The ST1 register contains CPU status flags including Z (zero), S (sign),
 *   C (carry), and O (overflow) flags. This instruction allows programs to
 *   save and examine the current processor status.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if ST1 is zero
 *   S = 1 if bit 31 of ST1 is set
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/St1Get.cs
 */
void nd500_instr_St1Get(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] St1Get at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = cpu->ST1;
    nd500_write_operand_word(cpu, &fi->operands[0], value);

    // NO status update. Storing ST1 to memory leaves the condition flags unchanged, matched to the
    // REAL B30 microcode: the store path STORST1 001107 -> 001110 -> READST1 (015017-015025) ->
    // 001112 (G,OOPS) runs with Status=0 on every word - there is no ST,SAVA anywhere (verified by a
    // microword single-step trace). Was setting Z/S from the stored value, which cleared Z whenever
    // ST1 was non-zero and diverged from the microword on every store-ST1.
}
