/*
 * Cwip.c - ND-500 Cwip instruction (SYSTEM class)
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
#include "nd500_page_bits.h"
#include <stdio.h>

/**
 * Cwip instruction - SYSTEM class
 *
 * CWIP - Clear Written In Page Table
 *
 * Format: CWIP
 *
 * Assembly:
 *   CWIP (clear WIP table)                   Hex 0xFF1B
 *
 * Operation: 0 -> entire WIP table
 *
 * Description:
 *   Privileged instruction that clears the entire Written In Page table.
 *   The WIP table tracks which pages have been written to and must be
 *   written back to disk before being replaced. This instruction is used
 *   by the swapper routines to reset the WIP tracking state.
 *
 *   Clears every bit of the WIP table kept in nd500_page_bits.c.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.19
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Cwip.cs
 */
void nd500_instr_Cwip(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 41-45) */
    if (fi->operand_count != 0) {
        printf("[ERROR] CWIP at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - CWIP requires privileged access (like C# lines 48-52) */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    nd500_page_bits_count(ND500_PAGE_OP_CWIP);
    nd500_page_bits_clear_all(cpu->machine, ND500_PAGE_TABLE_WIP);

    /* Data status bits: Unaffected (ND-05.009.4 16.19) */
}
