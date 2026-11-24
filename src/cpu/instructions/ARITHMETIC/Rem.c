/*
 * Rem.c - ND-500 Rem instruction (ARITHMETIC class)
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
/* The integer part of an F or D value as a value of the same type: the
 * mantissa bits below the units place are cleared (cut off towards zero,
 * 10.34). Value = 2**(e - 256) * 0.1mmm (7.2.5), so the units place is
 * (mb + 1) - (e - 256) bits up from the bottom. A magnitude below 1 gives 0. */
static uint64_t integer_part_of(uint64_t bits, int mb) {
    int e = (int)((bits >> mb) & 0x1FF);
    int fraction_bits = (mb + 1) - (e - 256);
    if (e == 0 || fraction_bits > mb) {
        return 0;
    }
    if (fraction_bits <= 0) {
        return bits;
    }
    return bits & ~((((uint64_t)1) << fraction_bits) - 1);
}

void nd500_instr_Rem(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count and target register (n from opcode low bits)
    if (!nd500_validate_operand_count(cpu, fi, 3, INSTR_REM)) return;
    if (!nd500_validate_target_register(cpu, fi, INSTR_REM)) return;

    bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
    uint8_t reg_num = fi->target_register;

    // Read <x> and <y> as ND-format bits. The arithmetic below is done in the
    // ND-500 format itself (float_exact.c), never through a host float or
    // double: those hold 24 and 53 bits against F's 23 and D's 55.
    uint64_t x, y;
    if (is_double) {
        x = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        y = nd500_read_operand_doubleword(cpu, &fi->operands[1]);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
    return;
    }
    } else {
        x = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type) & 0xFFFFFFFFull;
        y = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type) & 0xFFFFFFFFull;
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
    }

    // <y> = 0 -> DZ (manual 10.33); an exponent field of 0 is exactly zero (7.2.5 / 7.2.6)
    int mb = is_double ? 54 : 22;
    if (((y >> mb) & 0x1FF) == 0) {
        cpu->ST1 |= ND500_FLAG_DZ;
        trap_divide_by_zero(cpu, fi->address);
        return;
    }
    cpu->ST1 &= ~ND500_FLAG_DZ;

    // 10.33 "Algorithms used: <q> = int(<x>/<y>), Rn = <x> - <q>*<y>", each a
    // floating operation rounded by 7.2.7. The Note settles that the steps are
    // done in the float format: "Some precision may be lost in the subtraction
    // if the quotient <x>/<y> is large. If it is so large that the binary
    // point is outside the floating format, then int(<x>/<y>) = <x>/<y>, and
    // Rn will be zero." int() cuts off the fraction (10.34 INT, "No rounding is
    // performed"), which is exact in the format. Same as RetroCore Rem.cs.
    unsigned div_exc = 0, mul_exc = 0, sub_exc = 0;
    uint64_t quotient = nd500_fx_div(x, y, is_double, &div_exc);
    uint64_t q = integer_part_of(quotient, mb);
    uint64_t product = nd500_fx_mul(q, y, is_double, &mul_exc);
    uint64_t remainder = nd500_fx_sub(x, product, is_double, &sub_exc);

    // Overflow check on the quotient (the value being stored)
    if (div_exc & ND500_FX_FO) {
        trap_floating_overflow(cpu, fi->address);
        return;
    }

    // Convert results back to ND-500 format and store:
    // integer part of quotient -> <q> operand, remainder -> register n
    uint64_t rem_bits = remainder;
    if (is_double) {
        nd500_write_operand_value(cpu, &fi->operands[2], q, fi->data_type);
        nd500_write_double_register(cpu, reg_num, rem_bits);
    } else {
        nd500_write_operand_value(cpu, &fi->operands[2], q, fi->data_type);
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
