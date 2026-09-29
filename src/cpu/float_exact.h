/**
 * float_exact.h - exact ND-500 floating point arithmetic
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Add, subtract, multiply and divide on the raw register bits of single
 * (32-bit) and double (64-bit) floating point numbers, as ND-05.009.4
 * defines them:
 *
 *   7.2.5/7.2.6  sign, 9-bit exponent with bias 256, and a mantissa of
 *                22 (single) or 54 (double) stored bits plus one implicit
 *                bit; value = S * 2**(e-256) * M with 0.5 <= M < 1. "An
 *                operand with exponent = 0 is treated as exactly zero, with
 *                no respect to the sign nor the mantissa."
 *   7.2.7        with L the last kept mantissa bit, G the next bit and St
 *                the OR of all bits below G, "if G=1 and (St=1 or L=1)
 *                then add one to the least significant bit of mantissa".
 *   6.5.1        floating overflow stores "the largest possible floating
 *                point value ... with the sign of the result"; floating
 *                underflow stores zero "with the sign of the result".
 *
 * The result is formed exactly and rounded once. Host IEEE arithmetic
 * cannot do this: IEEE single rounds at 24 bits before the value is cut
 * to the ND-500's 23, and IEEE double keeps 53 bits against the 55.
 *
 * The ND-5000 does these operations on the AAP1 (the ND-570 floating
 * point unit, ND-05.022.1 5.2), so the microcoded mathematical functions
 * are sequences of these same operations.
 */

#ifndef ND500_FLOAT_EXACT_H
#define ND500_FLOAT_EXACT_H

#include <stdint.h>
#include <stdbool.h>

/* Unsigned 128-bit integer for the exact intermediate results. Compilers
 * for 64-bit hosts provide unsigned __int128; 32-bit hosts (32-bit ARM,
 * 32-bit x86 Windows) do not, and get a two-halves struct with the same
 * operations in float_exact.c. Defining ND500_U128_PORTABLE forces the
 * struct on any host, so the two versions can be compared. */
#if defined(__SIZEOF_INT128__) && !defined(ND500_U128_PORTABLE)
typedef unsigned __int128 nd500_u128;
static inline nd500_u128 nd500_u128_from_u64(uint64_t v) { return v; }
#else
typedef struct { uint64_t hi, lo; } nd500_u128;
static inline nd500_u128 nd500_u128_from_u64(uint64_t v) {
    nd500_u128 r;
    r.hi = 0;
    r.lo = v;
    return r;
}
#endif

/* Exceptions reported through the exc argument (OR-ed in, never cleared). */
#define ND500_FX_FO 1u   /* floating overflow */
#define ND500_FX_FU 2u   /* floating underflow */
#define ND500_FX_DZ 4u   /* divide by zero: the result is the dividend */

uint64_t nd500_fx_add(uint64_t a, uint64_t b, bool is_double, unsigned* exc);
uint64_t nd500_fx_sub(uint64_t a, uint64_t b, bool is_double, unsigned* exc);   /* a - b */
uint64_t nd500_fx_mul(uint64_t a, uint64_t b, bool is_double, unsigned* exc);
uint64_t nd500_fx_div(uint64_t a, uint64_t b, bool is_double, unsigned* exc);   /* a / b */

/* Square root, rounded once by 7.2.7. A negative operand (nonzero) sets
 * ND500_FX_IVO and returns 0: SQRTF @001477 and SQRTD @001501 go to IVOZRO
 * @020504, which zeroes the result and sets IVO. */
#define ND500_FX_IVO 8u  /* invalid operation */
uint64_t nd500_fx_sqrt(uint64_t a, bool is_double, unsigned* exc);

/* Exact value <-> bits, for results formed outside the four operations:
 * value = (negative ? -1 : 1) * magnitude * 2**scale, rounded once. */
uint64_t nd500_fx_round(bool negative, nd500_u128 magnitude, int scale,
                        bool sticky, bool is_double, unsigned* exc);

/* Split bits into sign, significand (0 when the exponent field is 0) and
 * scale, so that value = +/- significand * 2**scale. */
void nd500_fx_decode(uint64_t bits, bool is_double, bool* negative,
                     uint64_t* significand, int* scale);

#endif
