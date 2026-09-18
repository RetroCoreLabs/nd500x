/*
 * E1Set.c - ND-500 E1Set instruction (MOVE class)
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
 * E1Set instruction - MOVE class
 *
 * Load E1 Register (lower 32 bits): <source> -> E1[31:0]
 *
 * Variants: 1
 * Mnemonics: e1:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFE34
 *
 * Operation: <source> -> regs.E1[31:0]
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and stores it to the lower
 *   32 bits of the E1 extended register. Updates Z and S flags based on
 *   the word value.
 *
 *   Note: E-registers are 64-bit but this instruction only transfers to the
 *   lower 32 bits. Upper 32 bits remain unchanged.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if value is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/E1Set.cs
 */
void nd500_instr_E1Set(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] E1Set at PC=0x%08X: Expected 1 operand, got %u\n",
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

    // en:= writes the source into the E1 register. E is the HIGH 32 bits of
    // the D1 pair (microcode LOADE1 @001164: D,E1); the previous code
    // wrote the low half, which is A1, not E1.
    cpu->E[0] = value;

    /* Z and S from the value; C and O are reset (manual 6.5.1; the microcode's
     * ST,SAVA status save, e.g. READ_RFEND @012243, STOREA1 @001170). */
    nd500_set_flags_zs(cpu, value, ND500_DTYPE_WORD);
}
