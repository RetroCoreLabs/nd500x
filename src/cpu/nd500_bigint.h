/**
 * nd500_bigint.h - a small signed multi-word integer, for float_math.c
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * The mathematical functions (float_math.c) work in fixed point with
 * thousands of bits after the binary point; C has no integer type that
 * wide. This module gives just the operations they need, in plain C99 with
 * no library beyond the C standard library, so it builds the same for the
 * host, MinGW and WASM.
 *
 * A number is sign and magnitude: 32-bit limbs, least significant first.
 * Every number comes from one arena (nd500_bi_arena) and has the arena's
 * fixed capacity; nothing is freed one by one, the whole arena is freed at
 * once. An operation whose result would not fit sets the arena's overflow
 * flag and leaves a wrong value; the caller must check the flag
 * (nd500_bi_arena_failed) and treat the run as failed.
 *
 * Division and shifts follow the two rules float_math.c needs, named in
 * each function: "trunc" cuts toward zero, "floor" toward minus infinity.
 */

#ifndef ND500_BIGINT_H
#define ND500_BIGINT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t* d;     /* limbs, least significant first; d[n..cap-1] are not used */
    int n;           /* limbs in use; 0 is the value zero, and then neg is false */
    bool neg;        /* sign; never true for zero */
} nd500_bi;

#define ND500_BI_SCRATCH 8

typedef struct {
    uint32_t* store; /* all limbs of all numbers */
    nd500_bi* nums;  /* the numbers handed out by nd500_bi_new */
    int cap;         /* limbs per number */
    int count;       /* numbers handed out */
    int max_count;   /* numbers the store holds */
    bool failed;     /* a result did not fit, or the arena ran out of numbers */
    nd500_bi scratch[ND500_BI_SCRATCH];   /* inside the operations only (nd500_bigint.c) */
} nd500_bi_arena;

/* An arena of max_count numbers of at least bits bits each. Returns false
 * when the memory could not be had. */
bool nd500_bi_arena_init(nd500_bi_arena* a, int bits, int max_count);
void nd500_bi_arena_free(nd500_bi_arena* a);
static inline bool nd500_bi_arena_failed(const nd500_bi_arena* a) { return a->failed; }

/* A new number, value 0. On a full arena it sets failed and returns the
 * last scratch number (so the caller never gets NULL). */
nd500_bi* nd500_bi_new(nd500_bi_arena* a);

void nd500_bi_set_u64(nd500_bi_arena* a, nd500_bi* r, uint64_t v);
void nd500_bi_set_i64(nd500_bi_arena* a, nd500_bi* r, int64_t v);
void nd500_bi_copy(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x);

static inline bool nd500_bi_is_zero(const nd500_bi* x) { return x->n == 0; }
static inline int nd500_bi_sign(const nd500_bi* x) { return x->n == 0 ? 0 : (x->neg ? -1 : 1); }

/* Number of bits in |x| (0 for zero). */
int nd500_bi_bitlen(const nd500_bi* x);
/* Bit k of |x|. */
bool nd500_bi_testbit(const nd500_bi* x, int k);
/* True when any of the bits 0 .. k-1 of |x| is set. */
bool nd500_bi_low_bits_nonzero(const nd500_bi* x, int k);
/* The low 64 bits of |x|. */
uint64_t nd500_bi_low_u64(const nd500_bi* x);

int nd500_bi_cmp(const nd500_bi* x, const nd500_bi* y);      /* -1, 0, 1 */
int nd500_bi_cmpabs(const nd500_bi* x, const nd500_bi* y);   /* |x| against |y| */

/* r may be the same number as an operand in every operation below. */
void nd500_bi_add(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, const nd500_bi* y);
void nd500_bi_sub(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, const nd500_bi* y);
void nd500_bi_neg(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x);
void nd500_bi_abs(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x);
void nd500_bi_add_i64(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, int64_t k);
void nd500_bi_mul(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, const nd500_bi* y);
void nd500_bi_mul_i64(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, int64_t k);
void nd500_bi_shl(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, int k);
/* x / 2**k toward zero (the magnitude shifted, the sign kept). */
void nd500_bi_shr_trunc(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, int k);
/* x / 2**k toward minus infinity (an arithmetic shift). */
void nd500_bi_shr_floor(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, int k);
/* x / y toward zero; y must not be zero (failed is set when it is). */
void nd500_bi_div_trunc(nd500_bi_arena* a, nd500_bi* q, const nd500_bi* x, const nd500_bi* y);
/* x / k toward zero for 0 < k < 2**32. */
void nd500_bi_div_u32(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, uint32_t k);
/* The whole square root, floor(sqrt(x)), of x >= 0 (0 for x <= 0). */
void nd500_bi_isqrt(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x);

#endif
