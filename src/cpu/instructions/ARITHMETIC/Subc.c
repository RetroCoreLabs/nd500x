/*
 * Subc.c - ND-500 Subc instruction (ARITHMETIC class)
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
 * Subc instruction - ARITHMETIC class
 *
 * Subtract with Carry: Rn + C + ~<subtrahend> -> Rn
 *
 * Variants: 1 (word-only)
 * Mnemonics: Wn SUBC (n=1..4)
 * Operands: 1 (<subtrahend/r/t>)
 *
 * Opcodes:
 *   0xFE44-0xFE47 (W1 SUBC through W4 SUBC) - Word subtract with carry
 *
 * Operation: Rn = Rn + C + ~<subtrahend>
 *   Where C is the carry flag (0 or 1) and ~ is one's complement.
 *
 * Description:
 *   Adds the carry bit in the status register (treated as 0 or 1) and the
 *   one's complement of <subtrahend> to the contents of Rn.
 *   Used for multiple-precision subtraction.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if carry from MSB occurred
 *   O = 1 if signed overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *
 * Reference: ND-500 Reference Manual, Chapter 11.18
 */
void nd500_instr_Subc(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 46-50) */
    if (fi->operand_count != 1) {
        printf("[ERROR] SUBC at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read register value */
    uint32_t regValue = nd500_read_integer_register(cpu, fi->target_register);

    /* Read operand value */
    uint32_t subtrahend = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Get carry in - the current C flag value (0 or 1) */
    uint32_t carryIn = ((cpu->ST1 & ND500_FLAG_C) != 0) ? 1 : 0;

    /* SUBC per ND-500 spec: Rn = Rn + C + ~subtrahend */
    uint32_t onesComplement = ~subtrahend;
    uint64_t result64 = (uint64_t)regValue + (uint64_t)onesComplement + (uint64_t)carryIn;
    uint32_t result = (uint32_t)result64;

    /* Write result back to register */
    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Detect carry out - C=1 if there was a carry from MSB */
    bool carryOut = (result64 > 0xFFFFFFFF);

    /* Detect overflow for signed arithmetic */
    int32_t signedReg = (int32_t)regValue;
    int32_t signedSub = (int32_t)subtrahend;
    int32_t signedResult = (int32_t)result;
    bool overflow = ((signedReg >= 0 && signedSub < 0 && signedResult < 0) ||
                     (signedReg < 0 && signedSub >= 0 && signedResult >= 0));

    /* Update status flags */
    nd500_set_flags_zsco(cpu, result, ND500_DTYPE_WORD, carryOut, overflow);
}
