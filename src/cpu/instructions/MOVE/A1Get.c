/*
 * A1Get.c - ND-500 A1Get instruction (MOVE class)
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
 * A1Get instruction - MOVE class
 *
 * Store A1 Register: A1 -> <dest>
 *
 * Variants: 1
 * Mnemonics: a1=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFE38
 *
 * Operation: regs.A1 -> <dest>
 *
 * Description:
 *   Reads the A1 float register and stores it to the destination operand.
 *   Updates Z and S flags based on the register value.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if A1 is zero
 *   S = 1 if A1 sign bit is set
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/A1Get.cs
 */
void nd500_instr_A1Get(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 21-22) */
    if (fi->operand_count != 1) {
        printf("[ERROR] A1Get at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read A1 register (like C# line 24) */
    uint32_t value = nd500_read_float_register(cpu, 1);

    /* Write to destination (like C# line 25) */
    nd500_write_operand_word(cpu, &fi->operands[0], value);

    /* Z and S from the value; C and O are reset (manual 6.5.1; the microcode's
     * ST,SAVA status save, e.g. READ_RFEND @012243, STOREA1 @001170). */
    nd500_set_flags_zs(cpu, value, ND500_DTYPE_WORD);
}
