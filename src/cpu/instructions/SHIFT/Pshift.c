/*
 * Pshift.c - ND-500 PSHIFT instruction
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
 * PSHIFT - packed shift (opcode 0xFEB2), ND-05.009.4 section 17.6.
 *
 * Format: PSHIFT <source/r/BCD>, <dest/w/BCD>
 * Operation: <source> -> <dest>, scaled to <dest>
 *
 * Descriptors, operand checks, scaling, rounding, BCD overflow and the
 * status bits are shared by all decimal instructions: see bcd_helpers.c.
 */
void nd500_instr_Pshift(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    nd500_dec_execute_convert(cpu, fi, false, false, false);
}
