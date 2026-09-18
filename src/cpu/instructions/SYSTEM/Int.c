/*
 * Int.c - ND-500 Int instruction (SYSTEM class)
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
#include <stdint.h>

/**
 * Int instruction - SYSTEM class
 *
 * Variants: 8
 * Mnemonics: int (F1-F4, D1-D4)
 * Operands: 1
 *
 * Opcodes:
 *   0xFE60-0xFE63 (int) - F1-F4 INT (float integer part)
 *   0xFE64-0xFE67 (int) - D1-D4 INT (double integer part)
 *
 * Calculates truncated integer part of float/double and loads result
 * into register in float/double format (not integer format).
 */
void nd500_instr_Int(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* 0xFE64-0xFE67 are the Dn forms, the four below them the Fn forms. The F
     * operand is read as FLOAT: read as WORD, a register operand came from the
     * I bank, so F1 INT A2 used I2. The work is shared with INTR
     * and done exactly on the ND-500 bits (instruction_helpers.c). */
    bool is_double = (fi->opcode >= 0xFE64 && fi->opcode <= 0xFE67);
    nd500_execute_integer_part(cpu, fi, is_double, false);
}
