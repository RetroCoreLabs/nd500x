/**
 * float_math.h - the ND-500 single precision mathematical functions
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2, EXP, ALOG, ALOG2 and ALOG10 on F
 * operands, computed the way the B30 microcode (MICRO-5800-B30) computes
 * them: the same constants, the same argument reduction, the same
 * polynomials and the same order of operations, each floating operation
 * rounded as the AAP rounds it (float_exact.h). The results were checked
 * bit for bit against the ND5000 microword engine running the real
 * microcode on about 2,800 operands (every function's normal range, both
 * signs, and the edge cases below).
 *
 * The microcode compares magnitudes as integer bit patterns in places, so
 * an operand with exponent field 0 and a nonzero mantissa is not always
 * treated as zero; these functions follow the microcode there too.
 *
 * Known difference: the microcode divides through DIV_32 (DIVFI_00
 * @024175), a table reciprocal with integer multiplies whose final
 * rounding is not always the manual's (7.2.7); here the division is the
 * manual's exact one. About 1 in 100 quotients whose exact value lies just
 * above a rounding half (fraction 0.5 to 0.6 of the last place) come out
 * one unit higher than the microcode. TAN, ATAN, ATAN2, ASIN, ACOS, EXP and
 * the logarithms divide once or twice.
 */

#ifndef ND500_FLOAT_MATH_H
#define ND500_FLOAT_MATH_H

#include <stdint.h>

/* Each function returns the F result bits and ORs ND500_FX_IVO into *exc
 * when the microcode raises the invalid operation trap. */
uint32_t nd500_fm_sin(uint32_t x, unsigned* exc);
uint32_t nd500_fm_cos(uint32_t x, unsigned* exc);
uint32_t nd500_fm_tan(uint32_t x, unsigned* exc);
uint32_t nd500_fm_asin(uint32_t x, unsigned* exc);
uint32_t nd500_fm_acos(uint32_t x, unsigned* exc);
uint32_t nd500_fm_atan2(uint32_t y, uint32_t x, unsigned* exc);   /* ATAN is atan2(x, 1.0) */
uint32_t nd500_fm_exp(uint32_t x, unsigned* exc);
uint32_t nd500_fm_alog(uint32_t x, int base, unsigned* exc);     /* base 0 = e, 2, 10 */

#endif
