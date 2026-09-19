/*
 * Divide.c - ND-500 Divide instruction (ARITHMETIC class)
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
 * Divide instruction - ARITHMETIC class
 *
 * Divide Register by Operand: Rn / <operand> -> Rn
 *
 * Variants: 5
 * Mnemonics: / (divide)
 * Operands: 1 (<operand/r/t>)
 *
 * Opcodes:
 *   0xFC4C (BYn /) byte divide
 *   0xFC50 (Hn /)  halfword divide
 *   0x0078 (Wn /)  word divide
 *   0x007C (Fn /)  float divide
 *   0x0080 (Dn /)  double divide
 *
 * Operation: Rn / <operand> -> Rn
 *
 * Description:
 *   The contents of the specified register are divided by the operand.
 *   The quotient is stored in the register. The remainder is discarded.
 *   Division by zero causes a divide by zero trap.
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
 * Flags: Z (zero), S (sign), O (overflow), DZ (divide by zero)
 *   Z = 1 if quotient is zero
 *   S = 1 if sign bit is set
 *   O = 1 if overflow (MIN_VALUE / -1)
 *   DZ = 1 if divisor is zero
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Divide by zero (DZ)
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Basic Arithmetic)
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Divide.cs
 */
void nd500_instr_Divide(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] Divide at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] Divide at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double variants */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        uint8_t reg_num = fi->target_register;

        /* Rn / <b> -> Rn, formed exactly and rounded by the manual's rule
         * (ND-05.009.4 7.2.7, float_exact.h). An operand with exponent 0 is
         * "exactly zero, with no respect to the sign nor the mantissa" (7.2.5),
         * so such a divisor is a divide by zero and leaves Rn unchanged.
         *
         * Earlier versions forced the result to 0 with FO|O|Z above exponent
         * field 384 and to 0 below 128, thresholds their own comments called
         * unverified; the manual (6.5.1) stores the largest value on floating
         * overflow and a signed zero on floating underflow, at the 9-bit
         * exponent limits. */
        uint64_t a = nd500_read_float_reg(cpu, reg_num, is_double);
        uint64_t b = nd500_read_float_operand(cpu, &fi->operands[0], is_double);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        unsigned exc = 0;
        uint64_t r = nd500_fx_div(a, b, is_double, &exc);
        if (exc & ND500_FX_DZ) {
            cpu->ST1 |= ND500_FLAG_DZ;
            trap_divide_by_zero(cpu, fi->address);
            return;
        }
        cpu->ST1 &= ~ND500_FLAG_DZ;
        nd500_write_float_reg(cpu, reg_num, r, is_double);
        nd500_float_status(cpu, fi->address, r, exc, is_double);
        return;
    }

    // Integer division
    uint32_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t divisor = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    // Check for divide by zero
    if (divisor == 0) {
        /* Non-restoring integer divide hardware leaves the quotient SATURATED at the
         * LARGEST-MAGNITUDE value OF THE DIVIDEND'S SIGN before the DZ trap is taken -
         * NOT unconditionally max positive. Adjudicated against the real B30 microcode
         * (2026-07-26): divide-by-zero BRANCHES on the dividend sign at @024133
         * (COND,MSGN -> INTDN):
         *   - POSITIVE dividend -> @024134-024135 (OR A,BM14=DZ bit12 ST,LOAD):
         *     quotient = max positive (BY 0x7F, H 0x7FFF, W 0x7FFFFFFF), S = 0.
         *   - NEGATIVE dividend -> INTDN @024136-024140 (OR A,SARG=010200 = DZ bit12 +
         *     S bit7 ST,LOAD): quotient = MIN (BY 0x80, H 0x8000, W 0x80000000), S = 1.
         * Traced microword: -12/0 -> 0x80000000 S=1; +5/0 -> 0x7FFFFFFF S=0. The earlier
         * "always max positive" fix (commit ddda6ea/778d44e) only covered positive
         * dividends. S follows the saturated quotient's sign (= the dividend sign). */
        bool dividend_negative;
        uint32_t sat;
        switch (fi->data_type) {
            case ND500_DTYPE_BYTE:
                dividend_negative = (reg_value & 0x80u) != 0;
                sat = dividend_negative ? 0x00000080u : 0x0000007Fu;
                break;
            case ND500_DTYPE_HALFWORD:
                dividend_negative = (reg_value & 0x8000u) != 0;
                sat = dividend_negative ? 0x00008000u : 0x00007FFFu;
                break;
            case ND500_DTYPE_WORD:
            default:
                dividend_negative = (reg_value & 0x80000000u) != 0;
                sat = dividend_negative ? 0x80000000u : 0x7FFFFFFFu;
                break;
        }
        nd500_write_integer_register(cpu, fi->target_register, sat);
        if (dividend_negative) {
            cpu->ST1 |= ND500_FLAG_S;   /* microcode INTDN ST,LOAD SARG=010200 sets S */
        } else {
            cpu->ST1 &= ~ND500_FLAG_S;
        }
        cpu->ST1 |= ND500_FLAG_DZ;
        trap_divide_by_zero(cpu, fi->address);
        return;
    }

    cpu->ST1 &= ~ND500_FLAG_DZ;

    // Perform signed division with overflow detection
    int64_t quotient = 0;
    bool overflow = false;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            int8_t dividend = (int8_t)reg_value;
            int8_t div = (int8_t)divisor;
            // Check for overflow: MIN_VALUE / -1
            if (dividend == INT8_MIN && div == -1) {
                overflow = true;
                quotient = dividend;  // Keep original value
            } else {
                quotient = dividend / div;
            }
            break;
        }
        case ND500_DTYPE_HALFWORD: {
            int16_t dividend = (int16_t)reg_value;
            int16_t div = (int16_t)divisor;
            // Check for overflow: MIN_VALUE / -1
            if (dividend == INT16_MIN && div == -1) {
                overflow = true;
                quotient = dividend;  // Keep original value
            } else {
                quotient = dividend / div;
            }
            break;
        }
        case ND500_DTYPE_WORD: {
            int32_t dividend = (int32_t)reg_value;
            int32_t div = (int32_t)divisor;
            // Check for overflow: MIN_VALUE / -1
            if (dividend == INT32_MIN && div == -1) {
                overflow = true;
                quotient = dividend;  // Keep original value
            } else {
                quotient = dividend / div;
            }
            break;
        }
        default:
            printf("[ERROR] Divide at PC=0x%08X: Invalid data type %u\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    // Mask result to data type
    uint32_t masked_result = nd500_mask_to_datatype(quotient, fi->data_type);

    // Write back to register
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    // Update status flags; C is not named for "/" and is reset (manual 6.5.1)
    cpu->ST1 &= ~ND500_FLAG_C;
    if (masked_result == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
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
