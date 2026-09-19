/**
 * float_math.h - the ND-500 mathematical functions, single and double
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2, EXP, ALOG, ALOG2 and ALOG10 on F
 * and D operands, computed the way the B30 microcode (MICRO-5800-B30)
 * computes them: the same constants, the same argument reduction, the same
 * polynomials and the same order of operations, each floating operation
 * rounded as the AAP rounds it (float_exact.h).
 *
 * The F and the D routines in the microcode have the same shape (SINF_0
 * @025761 and SIND_0 @026004, and so on); they differ in the constant
 * tables, the length of the polynomials, the width every operation is
 * rounded to, and the argument reduction (XREDU_F @026106, XREDU_D
 * @026144). An F value is the D value with the same high word and a zero
 * low word, so one implementation serves both.
 *
 * The results were checked bit for bit against the ND5000 microword engine
 * running the real microcode: about 2,800 F operands and about 3,100 D
 * operands (every function's normal range, both signs, and the edge cases).
 * For the D run the engine was given two corrections the documentation
 * asks for (ND-05.022.1 6.1: Q and the F-bus shifted the same way in one
 * microword form one 64 bit shift; ND-05.022.1 4.2: OR,NE selects the
 * extension part of the double operand in the next microword).
 *
 * The microcode compares magnitudes as integer bit patterns in places, so
 * an operand with exponent field 0 and a nonzero mantissa is not always
 * treated as zero; these functions follow the microcode there too.
 *
 * Division: the microcode divides F through DIV_32 (DIVFI_00 @024175) and D
 * through DIV_64 @024400. DIV_32 estimates the quotient with a table
 * reciprocal, then forms the true remainder (dividend * 2**19 less
 * quotient * divisor, both low 32 bit products, @024241-@024244), steps the
 * quotient up or down until the remainder is in range (QUOT_UP @024250,
 * QUOT_DOWN @024246) and rounds on the dropped quotient bit and that
 * remainder (QUOT_RND @024254). DIV_64 divides bit by bit and ends in the
 * same kind of rounding step. Both are therefore the manual's exactly
 * rounded division (7.2.7), which is what is used here; a floating quotient
 * is never exactly halfway between two values. On 3,000 random F operand
 * pairs the engine gives exactly this once its two remainder products are
 * taken as low 32 bits; TAN, ATAN, ATAN2, ASIN, ACOS, EXP and the logarithms
 * divide once or twice.
 */

#ifndef ND500_FLOAT_MATH_H
#define ND500_FLOAT_MATH_H

#include <stdbool.h>
#include <stdint.h>

/* Each function takes and returns raw bits (an F value in the low 32 bits,
 * as in float_exact.h) and ORs ND500_FX_IVO into *exc when the microcode
 * raises the invalid operation trap. */
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

#endif
