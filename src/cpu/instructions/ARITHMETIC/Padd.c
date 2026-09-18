/*
 * Padd.c - ND-500 PADD instruction
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
 * PADD - packed add (opcode 0xFEB0), ND-05.009.4 section 17.2.
 *
 * Format: PADD <a/r/BCD>, <b/r/BCD>, <c/w/BCD>
 * Operation: <a> + <b> -> <c>
 *
 * Descriptors, operand checks, scaling, rounding, BCD overflow and the
 * status bits are shared by all decimal instructions: see bcd_helpers.c.
 */
void nd500_instr_Padd(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    nd500_dec_execute_arith(cpu, fi, ND500_DEC_ADD, false);
}
