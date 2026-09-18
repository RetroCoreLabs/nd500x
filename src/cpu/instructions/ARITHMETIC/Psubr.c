/*
 * Psubr.c - ND-500 PSUBR instruction
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
 * PSUBR - packed subtract rounded (opcode 0xFE86), ND-05.009.4 section 17.3.
 *
 * Format: PSUBR <a/r/BCD>, <b/r/BCD>, <c/w/BCD>
 * Operation: <a> - <b> -> <c>, rounded
 *
 * Descriptors, operand checks, scaling, rounding, BCD overflow and the
 * status bits are shared by all decimal instructions: see bcd_helpers.c.
 */
void nd500_instr_Psubr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    nd500_dec_execute_arith(cpu, fi, ND500_DEC_SUB, true);
}
