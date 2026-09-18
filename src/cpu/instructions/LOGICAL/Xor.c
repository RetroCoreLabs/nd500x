/*
 * Xor.c - ND-500 Xor instruction (LOGICAL class)
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
 * Xor instruction - LOGICAL class
 *
 * Bitwise XOR of register with operand. Rn = Rn XOR operand
 *
 * Variants: 4 (by data type and register)
 * Mnemonics: BIn XOR, BYn XOR, Hn XOR, Wn XOR (n=1..4)
 * Operands: 1 (value to XOR)
 *
 * Opcodes:
 *   0xFDF0-0xFDF3 (BI1 XOR through BI4 XOR) - Bit XOR
 *   0xFCA0-0xFCA3 (BY1 XOR through BY4 XOR) - Byte XOR
 *   0xFCA4-0xFCA7 (H1 XOR through H4 XOR) - Halfword XOR
 *   0x00A4-0x00A7 (W1 XOR through W4 XOR) - Word XOR
 *
 * Operation: Rn <- Rn XOR operand
 *
 * Description:
 *   A bitwise exclusive OR is performed between the contents of the specified
 *   register and the operand. The result is stored in the register. For BI, BY,
 *   and H data types, the upper part of the register is zero-filled.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *
 * Traps: Addressing traps only
 *
 * Reference: ND-500 Reference Manual and docs/instructions/asm/ (authoritative).
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/LOGICAL/Xor.cs
 */
void nd500_instr_Xor(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] XOR at PC=0x%08X: Expected 1 operand, got %u\n",
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

    /* Perform XOR operation */
    uint32_t result = (uint32_t)(reg_value ^ operand);

    /* Mask to data type (clears upper bits for BI, BY, H - like C# MaskToDataType) */
    result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to register */
    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Update status flags */
    nd500_set_flags_zs(cpu, result, fi->data_type);
}
