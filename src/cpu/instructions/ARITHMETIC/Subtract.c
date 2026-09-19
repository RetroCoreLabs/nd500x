/*
 * Subtract.c - ND-500 Subtract instruction (ARITHMETIC class)
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
 * Subtract instruction - ARITHMETIC class
 *
 * Subtract from Register: Rn - <operand> -> Rn
 *
 * Variants: 5
 * Mnemonics: - (subtract)
 * Operands: 1 (<operand/r/t>)
 *
 * Opcodes (register n in opcode bits 1-0):
 *   0xFC3C-0xFC3F (BY1- through BY4-) byte subtract
 *   0xFC40-0xFC43 (H1- through H4-)   halfword subtract
 *   0x0060-0x0063 (W1- through W4-)   word subtract
 *   0x0064-0x0067 (F1- through F4-)   float subtract
 *   0x0068-0x006B (D1- through D4-)   double subtract
 *
 * Operation: Rn - <operand> -> Rn
 *
 * Description:
 *   The operand is subtracted from the contents of the specified register.
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
 *   Z = 1 if difference is zero
 *   S = 1 if sign bit is set
 *   C = 1 if NO borrow (minuend >= subtrahend)
 *   O = 1 if overflow (integer or float overflow)
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Basic Arithmetic), and
 *            docs/instructions/asm/ (authoritative).
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Subtract.cs
 */
void nd500_instr_Subtract(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count and target register using helper functions
    if (!nd500_validate_operand_count(cpu, fi, 1, INSTR_SUBTRACT)) return;
    if (!nd500_validate_target_register(cpu, fi, INSTR_SUBTRACT)) return;

    /* Handle float/double variants */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        uint8_t reg_num = fi->target_register;

        if (reg_num < 1 || reg_num > 4) {
            printf("[ERROR] SUBTRACT at PC=0x%08X: Invalid register %u\n",
                   fi->address, reg_num);
            trap_illegal_operand(cpu, fi->address);
            return;
        }

        /* Read register value */
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
        uint64_t r = nd500_fx_sub(a, b, is_double, &exc);
        nd500_write_float_reg(cpu, reg_num, r, is_double);
        nd500_float_status(cpu, fi->address, r, exc, is_double);
        return;
    }

    // Integer subtraction
    uint32_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t operand = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    // Perform subtraction
    uint64_t result = reg_value - operand;

    /* ND-500 carry convention: C=1 means NO borrow (minuend >= subtrahend)
     * Reference: ND-500 Reference Manual Page 2040, SUBC formula Page 194 */
    bool carry = (reg_value >= operand);

    // Detect overflow
    bool overflow = nd500_detect_sub_overflow(reg_value, operand, result, fi->data_type);

    // Mask result to data type
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    // Write back to register
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    // Update status flags
    if (masked_result == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    if (carry) {
        cpu->ST1 |= ND500_FLAG_C;
    } else {
        cpu->ST1 &= ~ND500_FLAG_C;
    }

    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    // Set sign bit based on data type
    bool sign_bit = nd500_is_negative(masked_result, fi->data_type);
    if (sign_bit) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }
}
