/*
 * E4Set.c - ND-500 E4Set instruction (MOVE class)
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
 * E4Set instruction - MOVE class
 *
 * Load E4 Register (lower 32 bits): <source> -> E4[31:0]
 *
 * Variants: 1
 * Mnemonics: e4:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFE37
 *
 * Operation: <source> -> regs.E4[31:0]
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and stores it to the lower
 *   32 bits of the E4 extended register. Updates Z and S flags based on
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
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/E4Set.cs
 */
void nd500_instr_E4Set(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] E4Set at PC=0x%08X: Expected 1 operand, got %u\n",
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

    // en:= writes the source into the E4 register. E is the HIGH 32 bits of
    // the D4 pair (microcode LOADE4 @001167: D,E4); the previous code
    // wrote the low half, which is A4, not E4.
    cpu->E[3] = value;

    /* Z and S from the value; C and O are reset (manual 6.5.1; the microcode's
     * ST,SAVA status save, e.g. READ_RFEND @012243, STOREA1 @001170). */
    nd500_set_flags_zs(cpu, value, ND500_DTYPE_WORD);
}
