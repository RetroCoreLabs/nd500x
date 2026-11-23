/*
 * Axi.c - ND-500 AXI instruction (ARITHMETIC class)
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
#include "float_exact.h"

/**
 * AXI instruction - ARITHMETIC class
 *
 * AXI - A to the I'th Power (Floating Point Exponentiation)
 *
 * Format: tn AXI <a/r/t>, <i/r/W>
 *
 * Assembly:
 *   Fn AXI (float A to the I'th power)         Hex 0xFCC0 + (n-1)
 *   Dn AXI (double float A to the I'th power)  Hex 0xFCC4 + (n-1)
 *
 * Operation: <a>**<i> -> Rn
 *
 * Description:
 *   The floating-point operand <a> is raised to the power of the integer
 *   operand <i> and the result is stored in the specified floating-point
 *   register. The exponent <i> must be an integer value.
 *
 *   A negative <i> with <a> equal to zero "causes an illegal operand value
 *   trap condition and the result is set to the largest possible floating
 *   point number (approximately 5.8E+76)". When <i> is zero the result is one.
 *
 * Trap conditions (ND-05.009.4 12.1):
 *   - Addressing traps
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *   - Illegal operand value (IOV)
 *
 * Data status bits:
 *   - result = 0 -> Z
 *   - result.signbit -> S
 *   - floating underflow -> FU
 *   - floating overflow -> FO
 * (This header used to quote "Invalid operation (IVO)" and "invalid
 * operation -> IVO"; neither is in the manual.)
 *
 * Reference: ND-500 Reference Manual, Chapter 12.1 (Mathematical Functions)
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Axi.cs
 */
void nd500_instr_Axi(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] AXI at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check that we're using float registers */
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] AXI at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read floating-point operand a and integer operand i */
    uint64_t float_bits = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    int32_t exponent_i = (int32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* What ND-05.009.4 says for AXI: 12.1 "When <i> is zero, the result is
     * one"; "A negative value of <i> and the value of <a> equal to zero causes
     * an illegal operand value trap condition and the result is set to the
     * largest possible floating point number"; traps FO, FU, IOV. 6.5.1 FO:
     * "The largest possible floating point value will be stored in the
     * destination, with the sign of the result". 7.2.7: "After a floating point
     * operation, the result is normalized and the full mantissa is checked for
     * rounding" (rule: G=1 and (St=1 or L=1) adds one).
     * NOT DOCUMENTED (open, nd-lang docs/BUGS.md H21): HOW the power is formed -
     * the square-and-multiply loop, working in double for an F operand, taking
     * the reciprocal first for a negative <i>, and rounding to single only once
     * at the end. The manual gives no algorithm, so the last bit of F AXI for
     * large <i> is not settled by it. The steps below were taken from the
     * control store CONT-STORE-10611, which is not evidence (no working
     * microcode CPU exists to confirm the reading).
     * The control store (F AXI enters at 225B and jumps to 1553B; read
     * 2026-11-23, nd-lang BUGS.md H21 item 2) works in DOUBLE all the way, on
     * the FPU:
     *   1554B/1611B  n = 0 gives 1.0
     *   1555B/1614B  a = 0 gives 0 (with a negative n the IOV of manual 12.1)
     *   1561B        acc := 1.0; a negative n first takes the reciprocal of a
     *                through the divide sequence 1604B-1610B (ARMULA, BRMULP,
     *                APIMULA, PPIMULP, APIMUL: the FPU's double divide)
     *   1563B-1571B  bit 0 of n tested (AND BM#0); when set acc := a * acc
     *                (DOUBLE EX,MUL 1565B); n shifted (EX,SHL 1567B); when n
     *                is not yet 0, a := a * a (DOUBLE EX,MUL 1571B)
     *   1566B/1572B  a floating overflow leaves the loop for 1575B-1602B,
     *                which store the largest value with the sign
     *   1603B        EX?20 = EX,DTOFR, double to single with rounding (the
     *                same FPU step as D FCONR, nd500_fx_d_to_f)
     * The double multiply and divide are nd500_fx_mul / nd500_fx_div, the
     * ones D MUL and D DIV run in this emulator (rounded once by 7.2.7). The
     * FPU's own multiply rounding is not shown bit by bit in any document read
     * (NEC-01 page 69 breaks off at "In multiplications, some..."): that is an
     * open question for every D MUL here, not for AXI alone. The square at
     * 1571B is formed in the same word as the jump on n = 0; whether its FO
     * counts on the last pass (the delayed conditions, Micro Program Guide
     * 9.2) was not worked through, so the last square is not formed here.
     * D AXI (entry 226B) was not read; it runs the same loop without the
     * final conversion.
     * This replaced a host pow() on host doubles (53 bits for 55) followed by
     * a host float cast that rounded at the 24th significant bit. */
    const bool is_double = fi->data_type != ND500_DTYPE_FLOAT;
    const uint64_t one_d = (uint64_t)257 << 54;              /* 1.0 = 0.5 * 2**1 */
    uint64_t base = is_double ? float_bits : nd500_fx_f_to_d((uint32_t)float_bits);
    const bool base_zero = ((base >> 54) & 0x1FFu) == 0;     /* 7.2.6: exponent 0 is zero */
    const bool base_negative = (base >> 63) != 0;
    unsigned exc = 0;
    uint64_t acc;
    bool invalid_op = false;

    if (exponent_i == 0) {
        acc = one_d;
    } else if (base_zero) {
        if (exponent_i < 0) {
            /* Manual 12.1: "A negative value of <i> and the value of <a> equal
             * to zero causes an illegal operand value trap condition and the
             * result is set to the largest possible floating point number". */
            invalid_op = true;
        }
        acc = 0;
    } else {
        uint32_t n = (uint32_t)exponent_i;
        if (exponent_i < 0) {
            base = nd500_fx_div(one_d, base, true, &exc);
            n = 0u - n;                                      /* |n|, also for -2**31 */
        }
        acc = one_d;
        while ((exc & ND500_FX_FO) == 0) {
            if (n & 1u) {
                acc = nd500_fx_mul(base, acc, true, &exc);
                if (exc & ND500_FX_FO) {
                    break;
                }
            }
            n >>= 1;
            if (n == 0) {
                break;
            }
            base = nd500_fx_mul(base, base, true, &exc);
        }
        if (exc & ND500_FX_FO) {
            /* 1575B-1602B: the largest value with the sign of the result,
             * which is negative for a negative a and an odd n. */
            bool negative = base_negative && (((uint32_t)exponent_i & 1u) != 0);
            acc = (negative ? ((uint64_t)1 << 63) : 0) | 0x7FFFFFFFFFFFFFFFull;
            exc &= ~ND500_FX_FU;
        }
    }

    uint64_t result_bits;
    if (invalid_op) {
        /* The largest number: sign 0, exponent field 511, mantissa all ones. */
        result_bits = is_double ? 0x7FFFFFFFFFFFFFFFull : 0x7FFFFFFFu;
    } else if (is_double) {
        result_bits = acc;
    } else if (exc & ND500_FX_FO) {
        result_bits = (uint32_t)(acc >> 32);                 /* the largest single with the sign */
    } else {
        result_bits = nd500_fx_d_to_f(acc, &exc);            /* 1603B, EX,DTOFR */
    }

    if (is_double) {
        nd500_write_double_register(cpu, fi->target_register, result_bits);
    } else {
        nd500_write_float_register(cpu, fi->target_register, (uint32_t)result_bits);
    }

    /* Z and S from the result; FO and FU set and trapped as for every float
     * operation (6.5.1); C and O are not named, so they are reset. */
    nd500_float_status(cpu, fi->address, result_bits, exc, is_double);
    if (invalid_op) {
        raise_trap(cpu, TRAP_IOV, fi->address, 0);   /* illegal operand value (bit 16) */
    }
}
