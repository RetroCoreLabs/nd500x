/*
 * Add.c - ND-500 Add instruction (ARITHMETIC class)
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

        /* Read register and operand */
        double reg_value = 0.0;
        double operand_value = 0.0;

        if (is_double) {
            uint64_t reg_bits = nd500_read_double_register(cpu, reg_num);
            reg_value = nd500_double_to_ieee754(reg_bits);
            uint64_t op_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }
            operand_value = nd500_double_to_ieee754(op_bits);
        } else {
            uint32_t reg_bits = nd500_read_float_register(cpu, reg_num);
            reg_value = (double)nd500_float_to_ieee754(reg_bits);
            uint32_t op_bits = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }
            operand_value = (double)nd500_float_to_ieee754(op_bits);
        }

        /* Perform addition */
        double result = reg_value + operand_value;

        /* Check for overflow/underflow */
        if (isinf(result) || isnan(result)) {
            if (result > 0.0) {
                trap_floating_overflow(cpu, fi->address);
            } else if (result < 0.0) {
                trap_floating_overflow(cpu, fi->address);
            } else {
                trap_invalid_operation(cpu, fi->address);
            }
        } else if (result != 0.0 && fabs(result) < 1e-38) {
            /* Underflow - result too small */
            trap_floating_underflow(cpu, fi->address);
        }

        /* Convert result back to ND-500 format */
        uint64_t result_bits = 0;
        if (is_double) {
            result_bits = nd500_double_from_ieee754(result);
            nd500_write_double_register(cpu, reg_num, result_bits);
        } else {
            result_bits = nd500_float_from_ieee754((float)result);
            nd500_write_float_register(cpu, reg_num, (uint32_t)result_bits);
        }

        /* Update flags: Z (zero), S (sign) */
        if (is_double) {
            if (nd500_double_is_zero(result_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_Z);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_Z);
            }
            if (nd500_double_is_negative(result_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_S);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_S);
            }
        } else {
            uint32_t float_bits = (uint32_t)result_bits;
            if (nd500_float_is_zero(float_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_Z);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_Z);
            }
            if (nd500_float_is_negative(float_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_S);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_S);
            }
        }

        /* C flag unaffected for float operations */
        /* O flag unaffected (overflow handled by FO trap) */
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
