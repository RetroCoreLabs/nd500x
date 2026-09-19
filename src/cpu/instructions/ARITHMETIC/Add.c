/*
 * Add.c - ND-500 Add instruction (ARITHMETIC class)
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
#include "float_exact.h"
#include <stdio.h>
#include <math.h>

/**
 * Add instruction - ARITHMETIC class
 *
 * Add Operand to Register: Rn + <operand> -> Rn
 *
 * Variants: 5
 * Mnemonics: + (add)
 * Operands: 1 (<operand/r/t>)
 *
 * Opcodes:
 *   0xFC34 (BYn +) byte add
 *   0xFC38 (Hn +)  halfword add
 *   0x0054 (Wn +)  word add
 *   0x0058 (Fn +)  float add
 *   0x005C (Dn +)  double add
 *
 * Operation: Rn + <operand> -> Rn
 *
 * Description:
 *   The operand is added to the contents of the specified register.
 *   The result is stored in the register.
 *
 *   Register selection is encoded in opcode bits 1-0:
 *   - 00 -> register 1 (I1, A1)
 *   - 01 -> register 2 (I2, A2)
 *   - 10 -> register 3 (I3, A3)
 *   - 11 -> register 4 (I4, A4)
 *
 *   For integer variants: Uses I1-I4 registers
 *   For float/double: Uses A1-A4 (float) or D1-D4 (A+E pairs, double)
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if sum is zero
 *   S = 1 if sign bit is set
 *   C = 1 if carry from most significant bit (integer only)
 *   O = 1 if overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Basic Arithmetic)
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Add.cs
 */
void nd500_instr_Add(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] ADD at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double variants */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        uint8_t reg_num = fi->target_register;

        if (reg_num < 1 || reg_num > 4) {
            printf("[ERROR] ADD at PC=0x%08X: Invalid register %u\n",
                   fi->address, reg_num);
            trap_illegal_operand(cpu, fi->address);
            return;
        }

        uint64_t a = nd500_read_float_reg(cpu, reg_num, is_double);
        uint64_t b = nd500_read_float_operand(cpu, &fi->operands[0], is_double);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        /* Exact result, rounded by the manual's rule (float_exact.h). */
        unsigned exc = 0;
        uint64_t r = nd500_fx_add(a, b, is_double, &exc);
        nd500_write_float_reg(cpu, reg_num, r, is_double);
        nd500_float_status(cpu, fi->address, r, exc, is_double);
        return;
    }

    /* Read register and operand (like C# ReadIntegerRegister + ReadOperandValue) */
    uint64_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t operand = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Perform addition */
    uint64_t result = reg_value + operand;

    /* Detect carry and overflow before masking (using helpers to avoid duplication) */
    bool carry = nd500_detect_carry_add(result, fi->data_type);
    bool overflow = nd500_detect_add_overflow(reg_value, operand, result, fi->data_type);

    /* Mask to data type (like C# MaskToDataType) */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to register */
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    /* Update status flags: Z, S, C, O */
    nd500_set_flags_zsco(cpu, masked_result, fi->data_type, carry, overflow);
}
