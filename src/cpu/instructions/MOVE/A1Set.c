/*
 * A1Set.c - ND-500 A1Set instruction (MOVE class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * A1Set instruction - MOVE class
 *
 * Load A1 Register: <source> -> A1
 *
 * Variants: 1
 * Mnemonics: a1:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFE30
 *
 * Operation: <source> -> regs.A1
 *
 * Description:
 *   Reads the source operand and stores it to the A1 float register.
 *   Updates Z and S flags based on the value.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if value is zero
 *   S = 1 if value sign bit is set
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/A1Set.cs
 */
void nd500_instr_A1Set(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] A1Set at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_operand_word(cpu, &fi->operands[0]);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    nd500_write_float_register(cpu, 1, value);

    if (value == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    if ((value & 0x80000000) != 0) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }
}
