/*
 * Wpconv.c - ND-500 WPCONV instruction
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "bcd_helpers.h"

/**
 * WPCONV - convert binary word to packed (opcode 0xFEB8+(n-1)), ND-05.009.4 section 17.10.
 *
 * Format: Wn WPCONV <dest/w/BCD>
 * Operation: Rn -> <dest>
 *
 * Descriptors, operand checks, scaling, rounding, BCD overflow and the
 * status bits are shared by all decimal instructions: see bcd_helpers.c.
 */
void nd500_instr_Wpconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    nd500_dec_execute_wpconv(cpu, fi);
}
