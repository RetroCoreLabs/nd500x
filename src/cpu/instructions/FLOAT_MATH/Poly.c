/*
 * Poly.c - ND-500 POLY instruction (FLOAT_MATH class)
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
#include "float_math.h"
#include "float_exact.h"
#include <stdio.h>

/**
 * POLY instruction - FLOAT_MATH class
 *
 * Evaluates a polynomial using Horner's method:
 *   result = c[m]*x^m + c[m-1]*x^(m-1) + ... + c[1]*x + c[0]
 *
 * Horner's algorithm (more efficient):
 *   result = c[m]
 *   for i = m-1 down to 0:
 *       result = result * x + c[i]
 *
 * Opcodes:
 *   0xFCE0-0xFCE3: Fn POLY (single-precision float, F1-F4)
 *   0xFCE4-0xFCE7: Dn POLY (double-precision float, D1-D4)
 *
 * Operands:
 *   Operand 0: X value (independent variable)
 *   Operand 1: Degree m (constant byte, 0-255)
 *   Extra operands: m+1 coefficients in cpu->extra_operands[] (c[m], c[m-1], ..., c[0])
 *
 * Status flags:
 *   Z: Set if result == 0.0
 *   S: Set if result < 0.0
 *   FO: Set if floating overflow occurred
 *   FU: Set if floating underflow occurred
 *
 * The value is worked out on the exact operands and rounded once
 * (nd500_fm_poly, float_math.c). Until 2026-11-24 it went through a host
 * double, 53 bits against D's 55, and an F result through a host float,
 * 24 bits against F's 23.
 */
void nd500_instr_Poly(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* 1. Validate operand count: 2 fixed operands (x, degree) plus the
     * coefficients, which the decoder appends to fi->operands AND stores
     * in cpu->extra_operands */
    if (fi->operand_count < 2) {
        printf("[ERROR] POLY at PC=0x%08X: Expected >=2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* 2. Determine data type (F=float, D=double) from data_type */
    bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);

    /* 3. Read X value (operand 0) */
    uint64_t x_bits = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* 4. Read degree m (operand 1 - must be constant byte) */
    uint8_t m = nd500_read_operand_byte(cpu, &fi->operands[1]);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* 5. Validate coefficient count (m+1 coefficients expected) */
    uint16_t expected_coeffs = (uint16_t)m + 1;
    if (cpu->extra_operand_count != expected_coeffs) {
        printf("[ERROR] POLY at PC=0x%08X: Expected %u coefficients (degree %u), got %u\n",
               fi->address, expected_coeffs, m, cpu->extra_operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* 6. The F operands are the low 32 bits */
    if (!is_double) {
        x_bits &= 0xFFFFFFFFull;
    }

    /* 7. Horner's method evaluation
     *
     * Coefficients are stored in order: c[m], c[m-1], ..., c[1], c[0]
     * extra_operands[0] = c[m]   (highest degree coefficient)
     * extra_operands[1] = c[m-1]
     * ...
     * extra_operands[m] = c[0]   (constant term)
     */
    uint64_t coeffs[256];

    for (uint16_t i = 0; i <= m; i++) {
        /* Read coefficient from extra_operands */
        uint64_t coeff_bits = nd500_read_operand_value(cpu,
            &cpu->extra_operands[i], fi->data_type);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        coeffs[i] = is_double ? coeff_bits : (coeff_bits & 0xFFFFFFFFull);
    }

    /* 8. The polynomial, rounded once (6.5.1 values on FO / FU) */
    unsigned exc = 0;
    uint64_t result_bits = nd500_fm_poly(x_bits, coeffs, (int)m + 1, is_double, &exc);

    /* 9. Write the result to register n */
    if (is_double) {
        nd500_write_double_register(cpu, fi->target_register, result_bits);
    } else {
        nd500_write_float_register(cpu, fi->target_register, (uint32_t)result_bits);
    }

    /* 10. Status: "The Z and S bits reflect the final result" (12.3); an
     * exponent field of 0 is exactly zero (7.2.5 / 7.2.6). */
    int mb = is_double ? 54 : 22;
    nd500_clear_flag(cpu, ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_FO | ND500_FLAG_FU);
    if (((result_bits >> mb) & 0x1FF) == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    }
    if ((result_bits >> (mb + 9)) & 1) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    }
    if (exc & ND500_FX_FO) {
        nd500_set_flag(cpu, ND500_FLAG_FO);
    }
    if (exc & ND500_FX_FU) {
        nd500_set_flag(cpu, ND500_FLAG_FU);
    }
}
