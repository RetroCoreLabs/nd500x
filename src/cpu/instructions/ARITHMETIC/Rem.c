/*
 * Rem.c - ND-500 Rem instruction (ARITHMETIC class)
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
 * Rem instruction - ARITHMETIC class
 *
 * Floating Point Remainder (float divide with remainder)
 *
 * Format: tn REM <x/r/t>,<y/r/t>,<q/w/t>
 *
 * Variants: 8 (F and D only - REM has NO integer variants)
 * Mnemonics: Fn REM, Dn REM
 * Operands: 3 (<x/r/t>, <y/r/t>, <q/w/t>)
 *
 * Opcodes (manual 10.33: 177130B-177137B):
 *   0xFE58-0xFE5B (Fn REM) - float, remainder to A1-A4
 *   0xFE5C-0xFE5F (Dn REM) - double, remainder to D1-D4 (An:En pair)
 *
 * Operation (manual 10.33):
 *   <q> = int(<x>/<y>)          integer part of quotient, in float format
 *   Rn  = <x> - <q>*<y>         remainder, in float format, to register n
 *
 * Description:
 *   The <x> operand is divided by the <y> operand. The integer part of the
 *   quotient (in float format) is stored in <q>. The remainder is loaded
 *   into float register n (A-register for F, A:E pair for D), where n is
 *   encoded in the low 2 bits of the opcode.
 *
 *   Note (manual): precision may be lost in the subtraction if the quotient
 *   is large; if the binary point falls outside the floating format then
 *   int(x/y) = x/y and Rn becomes zero.
 *
 * Flags (manual 10.33):
 *   Z  = 1 if remainder is zero
 *   S  = remainder sign bit
 *   FU = floating underflow
 *   FO = floating overflow
 *   DZ = 1 if <y> is zero
 *
 * Trap conditions:
 *   Addressing traps, Floating overflow (FO), Floating underflow (FU),
 *   Divide by zero (DZ)
 *
 * Reference: ND-500 Reference Manual, section 10.33 (Floating point remainder)
 *            docs/instructions/asm/rem.md
 */
void nd500_instr_Rem(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count and target register (n from opcode low bits)
    if (!nd500_validate_operand_count(cpu, fi, 3, INSTR_REM)) return;
    if (!nd500_validate_target_register(cpu, fi, INSTR_REM)) return;

    bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
    uint8_t reg_num = fi->target_register;

    // Read <x> and <y> as ND-format bits, convert to IEEE for arithmetic
    double x, y;
    if (is_double) {
        x = nd500_double_to_ieee754(nd500_read_operand_doubleword(cpu, &fi->operands[0]));
        y = nd500_double_to_ieee754(nd500_read_operand_doubleword(cpu, &fi->operands[1]));
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
    return;
    }
    } else {
        x = (double)nd500_float_to_ieee754((uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type));
        y = (double)nd500_float_to_ieee754((uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type));
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
    }

    // <y> = 0 -> DZ (manual 10.33)
    if (y == 0.0) {
        cpu->ST1 |= ND500_FLAG_DZ;
        trap_divide_by_zero(cpu, fi->address);
        return;
    }
    cpu->ST1 &= ~ND500_FLAG_DZ;

    // Manual algorithm: q = int(x/y); Rn = x - q*y
    double quotient = trunc(x / y);
    double remainder = x - quotient * y;

    // Overflow check on the quotient (the value being stored)
    if (isinf(quotient) || isnan(quotient)) {
        trap_floating_overflow(cpu, fi->address);
        return;
    }

    // Convert results back to ND-500 format and store:
    // integer part of quotient -> <q> operand, remainder -> register n
    uint64_t rem_bits;
    if (is_double) {
        nd500_write_operand_value(cpu, &fi->operands[2],
                                  nd500_double_from_ieee754(quotient), fi->data_type);
        rem_bits = nd500_double_from_ieee754(remainder);
        nd500_write_double_register(cpu, reg_num, rem_bits);
    } else {
        nd500_write_operand_value(cpu, &fi->operands[2],
                                  (uint64_t)nd500_float_from_ieee754((float)quotient), fi->data_type);
        rem_bits = nd500_float_from_ieee754((float)remainder);
        nd500_write_float_register(cpu, reg_num, (uint32_t)rem_bits);
    }

    // Data status bits: remainder = 0 -> Z, remainder sign -> S
    bool rem_zero = is_double ? nd500_double_is_zero(rem_bits)
                              : nd500_float_is_zero((uint32_t)rem_bits);
    bool rem_neg = is_double ? nd500_double_is_negative(rem_bits)
                             : nd500_float_is_negative((uint32_t)rem_bits);

    if (rem_zero) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }
    if (rem_neg) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }
}
