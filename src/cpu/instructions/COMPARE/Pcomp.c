/*
 * Pcomp.c - ND-500 PCOMP instruction
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
 * PCOMP - packed compare (opcode 0xFEB3), ND-05.009.4 section 17.5.
 *
 * Format: PCOMP <a/r/BCD>, <b/r/BCD>
 * Operation: status from <a> - <b>; the result is discarded
 *
 * Descriptors, operand checks, scaling, rounding, BCD overflow and the
 * status bits are shared by all decimal instructions: see bcd_helpers.c.
 */
void nd500_instr_Pcomp(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    nd500_dec_execute_compare(cpu, fi);
}
