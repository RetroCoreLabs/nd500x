/*
 * Pwconv.c - ND-500 PWCONV instruction
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
 * PWCONV - convert packed to binary word (opcode 0xFEBC+(n-1)), ND-05.009.4 section 17.9.
 *
 * Format: Wn PWCONV <source/r/BCD>
 * Operation: <source> -> Rn, fraction dropped
 *
 * Descriptors, operand checks, scaling, rounding, BCD overflow and the
 * status bits are shared by all decimal instructions: see bcd_helpers.c.
 */
void nd500_instr_Pwconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    nd500_dec_execute_pwconv(cpu, fi);
}
