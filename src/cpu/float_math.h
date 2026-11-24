/**
 * float_math.h - the ND-500 mathematical functions, single and double
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2, EXP, ALOG, ALOG2 and ALOG10 on F
 * and D operands. ND-05.009.4 chapter 12 names each function ("the
 * trigonometric sine", "the natural logarithm", ...) and gives no algorithm
 * and no rounding rule for it. Owner decision 2026-11-24: the result is the
 * true value of the function rounded once to the format by the 7.2.7 rule
 * ("if G=1 and (St=1 or L=1) then add one to the least significant bit of
 * mantissa"), for F (23 significant bits) and D (55).
 *
 * float_math.c computes it in fixed point with an error bound carried
 * through every step, and doubles the precision until both ends of the
 * bound round to the same word (the method is described there). It is the
 * same method as RetroCore ND500FloatMath.cs. Before the decision this file
 * followed the ND-5000 B30 microcode polynomials, which are one last bit
 * off the true value for some operands.
 *
 * Kept from the earlier code: the operand checks and the values the manual
 * gives for an invalid operand (SIN, COS, TAN above 65536: IVO and 0; ASIN,
 * ACOS outside -1..+1: IVO and 0; ATAN2 of 0, 0: IVO and 0; EXP above about
 * 176.75 = 255 ln 2: IVO and the largest value, below minus that: 0; ALOG of
 * zero or a negative number: IVO and all bits set).
 */

#ifndef ND500_FLOAT_MATH_H
#define ND500_FLOAT_MATH_H

#include <stdbool.h>
#include <stdint.h>

/* Each function takes and returns raw bits (an F value in the low 32 bits,
 * as in float_exact.h) and ORs ND500_FX_IVO into *exc when the invalid
 * operation trap condition arises. */
uint64_t nd500_fm_sin(uint64_t x, bool is_double, unsigned* exc);
uint64_t nd500_fm_cos(uint64_t x, bool is_double, unsigned* exc);
uint64_t nd500_fm_tan(uint64_t x, bool is_double, unsigned* exc);
uint64_t nd500_fm_asin(uint64_t x, bool is_double, unsigned* exc);
uint64_t nd500_fm_acos(uint64_t x, bool is_double, unsigned* exc);
uint64_t nd500_fm_atan2(uint64_t y, uint64_t x, bool is_double, unsigned* exc);   /* ATAN is atan2(x, 1.0) */
uint64_t nd500_fm_exp(uint64_t x, bool is_double, unsigned* exc);
uint64_t nd500_fm_alog(uint64_t x, int base, bool is_double, unsigned* exc);     /* base 0 = e, 2, 10 */

/* 1.0 as raw bits, the second operand that makes ATAN out of ATAN2. */
uint64_t nd500_fm_one(bool is_double);

/* POLY (12.3): <cm>*x**m + ... + <c1>*x + <c0>, worked out on the exact
 * operand values (no rounding at any step) and rounded once by 7.2.7, with
 * the 6.5.1 out-of-range results (ND500_FX_FO / ND500_FX_FU in *exc). coeffs
 * holds the count = m + 1 coefficients from <cm> down to <c0>, F values in
 * the low 32 bits. The same method as RetroCore Poly.cs. */
uint64_t nd500_fm_poly(uint64_t x, const uint64_t* coeffs, int count, bool is_double,
                       unsigned* exc);

#endif
