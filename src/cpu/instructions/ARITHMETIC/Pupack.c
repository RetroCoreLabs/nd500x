/*
 * Pupack.c - ND-500 PUPACK instruction
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
 * PUPACK - convert packed to ASCII (opcode 0xFEB6), ND-05.009.4 section 17.8.
 *
 * Format: PUPACK <source/r/BCD>, <dest/w/ASCII>
 * Operation: <source> -> <dest>
 *
 * Descriptors, operand checks, scaling, rounding, BCD overflow and the
 * status bits are shared by all decimal instructions: see bcd_helpers.c.
 */
void nd500_instr_Pupack(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    nd500_dec_execute_convert(cpu, fi, false, true, false);
}
