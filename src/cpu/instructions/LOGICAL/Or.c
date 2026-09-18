/*
 * Or.c - ND-500 Or instruction (LOGICAL class)
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
 * Or instruction - LOGICAL class
 *
 * Bitwise OR of register with operand. Rn = Rn OR operand
 *
 * Variants: 4 (by data type and register)
 * Mnemonics: BIn OR, BYn OR, Hn OR, Wn OR (n=1..4)
 * Operands: 1 (value to OR)
 *
 * Opcodes:
 *   0xFDF8-0xFDFB (BI1 OR through BI4 OR) - Bit OR
 *   0xFC98-0xFC9B (BY1 OR through BY4 OR) - Byte OR
 *   0xFC9C-0xFC9F (H1 OR through H4 OR) - Halfword OR
 *   0x00A0-0x00A3 (W1 OR through W4 OR) - Word OR
 *
 * Operation: Rn <- Rn OR operand
 *
 * Description:
 *   A bitwise OR is performed between the contents of the specified register
 *   and the operand. The result is stored in the register. For BI, BY, and H
 *   data types, the upper part of the register is zero-filled.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *
 * Traps: Addressing traps only
 *
 * Reference: ND-500 Reference Manual and docs/instructions/asm/ (authoritative).
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/LOGICAL/Or.cs
 */
void nd500_instr_Or(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] OR at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read register and operand (like C# ReadIntegerRegister + ReadOperandValue) */
    uint32_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t operand = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Perform OR operation */
    uint32_t result = (uint32_t)(reg_value | operand);

    /* Mask to data type (clears upper bits for BI, BY, H - like C# MaskToDataType) */
    result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to register */
    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Update status flags */
    nd500_set_flags_zs(cpu, result, fi->data_type);
}
