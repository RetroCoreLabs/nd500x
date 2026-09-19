/*
 * Svers.c - ND-500 Svers instruction (SYSTEM class)
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
 * Svers instruction - SYSTEM class
 *
 * SVERS - Store Version
 *
 * Format: SVERS <destination/w/W>
 *
 * Assembly:
 *   SVERS (store version)                    Hex 0xFFFB
 *
 * Operation: <VERSION> -> <destination>
 *
 * Description:
 *   Store version number in destination address. The version number
 *   identifies the specific implementation of the ND-500 CPU architecture.
 *   This instruction is used by system software to determine which
 *   features and capabilities are available.
 *
 * Trap conditions: Addressing traps
 *
 * Data status bits: "Status bit set according to version": Z and S from it,
 *   C and O reset (ST,SAVA in SAVE_RES @012011).
 *
 * Reference: ND-500 Reference Manual, Chapter 16.35
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Svers.cs
 */
void nd500_instr_Svers(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 41-45) */
    if (fi->operand_count != 1) {
        printf("[ERROR] SVERS at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* The microprogram version: SVERS @001051 -> SVERS_1 @012007 calls
     * VERSION @000001, which loads the long argument 0x00002E9A, and SAVE_RES
     * @012011 stores it with ST,SAVA (Z and S from it, C and O reset). nd500x
     * is checked against the B30 image, so it reports the B30 version; the
     * C# port's 0x00010000 was made up. */
    const uint32_t VERSION_NUMBER = 0x00002E9Au;
    nd500_write_operand_value(cpu, &fi->operands[0], VERSION_NUMBER, ND500_DTYPE_WORD);
    nd500_set_flags_zs(cpu, VERSION_NUMBER, ND500_DTYPE_WORD);
}
