/*
 * Dcc.c - ND-500 Dcc instruction (SYSTEM class)
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
 * Dcc instruction - SYSTEM class
 *
 * DCC - Data Cache Clear
 *
 * Format: DCC
 *
 * Assembly:
 *   DCC (data cache clear)                    Hex 0xFF15
 *
 * Operation: Clear data cache
 *
 * Description:
 *   Privileged instruction that clears the data cache. This instruction
 *   is used to invalidate all entries in the data cache, forcing all
 *   subsequent data accesses to go to main memory. This is typically
 *   used during system initialization or when cache coherency needs to
 *   be maintained in multiprocessor systems.
 *
 *   EMULATOR NOTE: This is a no-op in the emulator since we don't have a
 *   separate data cache. All data accesses go directly to memory.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Dcc.cs
 */
void nd500_instr_Dcc(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 40-45) */
    if (fi->operand_count != 0) {
        printf("[ERROR] DCC at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* NOT PRIVILEGED - guard removed 2026-08-09.
     *
     * Manual ch.16.10 Data cache clear: the entry has NO "Privileged instruction" line in its
     * Description, and gives "Trap conditions: None". That line is where the
     * manual records privilege - 15.17 CLINIT and 16.13 DMON both carry it and
     * both still say "Trap Conditions: None", so the trap list is not the
     * marker.
     *
     * The ND-5000 control store agrees: DCC (000720) jumps straight to
     * DCC_IC (012275) and on to the cache-clear subroutines, with no PIA test
     * anywhere in the path.
     *
     * Found by the sweep prompted by the same bug in TUTTI: a guard that came
     * from the C# port rather than from any source. The comment here even said
     * "(like C# lines 48-53)", naming the origin.
     */

    /* EMULATOR NO-OP: Data cache clear is not implemented in emulator */
    /* In hardware, this would clear the data cache to force subsequent */
    /* data accesses to go to main memory for cache coherency */

    /* No status bits affected for this instruction */
}
