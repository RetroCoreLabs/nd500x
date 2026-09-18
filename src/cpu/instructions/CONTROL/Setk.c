/*
 * Setk.c - ND-500 Setk instruction (CONTROL class)
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
 * Setk instruction - CONTROL class
 *
 * Set K Flag: 1 -> K
 *
 * Variants: 1
 * Mnemonics: setk
 * Operands: 0 (no operands)
 *
 * Opcode: 0xFE02
 *
 * Operation: 1 -> ST.K
 *
 * Description:
 *   Sets the K (Destination Full) bit of the status register to 1.
 *
 *   The K flag is used for:
 *   - Signaling between subroutines (RETK returns with K=1)
 *   - Boolean results (found/not found, success/failure)
 *   - Conditional returns (IF K RET)
 *   - Synchronization
 *   - General-purpose flag
 *
 * Flags: K (Destination Full)
 *   K = 1 (always set)
 *
 * Trap conditions:
 *   - None
 *
 * Reference: ND-500 Reference Manual, Chapter 15.11
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CONTROL/Setk.cs
 */
void nd500_instr_Setk(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    (void)fi;  // Suppress unused parameter warning

    // Set K flag in status register
    cpu->ST1 |= ND500_FLAG_K;
}
