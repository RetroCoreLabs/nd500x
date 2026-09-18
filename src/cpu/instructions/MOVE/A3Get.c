/*
 * A3Get.c - ND-500 A3Get instruction (MOVE class)
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
 * A3Get instruction - MOVE class
 *
 * Store A3 Register: A3 -> <dest>
 *
 * Variants: 1
 * Mnemonics: a3=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFE3A
 *
 * Operation: regs.A3 -> <dest>
 *
 * Description:
 *   Reads the A3 float register and stores it to the destination operand.
 *   Updates Z and S flags based on the register value.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if A3 is zero
 *   S = 1 if A3 sign bit is set
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 */
void nd500_instr_A3Get(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] A3Get at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_float_register(cpu, 3);
    nd500_write_operand_word(cpu, &fi->operands[0], value);

    /* Z and S from the value; C and O are reset (manual 6.5.1; the microcode's
     * ST,SAVA status save, e.g. READ_RFEND @012243, STOREA1 @001170). */
    nd500_set_flags_zs(cpu, value, ND500_DTYPE_WORD);
}
