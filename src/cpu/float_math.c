/**
 * float_math.c - the ND-500 mathematical functions, single and double
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * See float_math.h for what is computed and why. This file is the same
 * method, step for step, as RetroCore's ND500FloatMath.cs:
 *
 * Every value is a fixed-point integer V with P bits after the binary
 * point (value = V * 2**-P), carried together with a bound E on its error
 * in units of 2**-P (|true value - V| <= E). The bound is worked out at
 * every step: add, the sum of the two bounds; multiply and divide, the
 * first-order terms plus the cut-off; a series, the slope of the function
 * times the argument's error plus the cut-off of every term and the tail.
 * The result is taken only when the low end (V - E) and the high end
 * (V + E) round to the same ND-500 word by the 7.2.7 rule: rounding is
 * monotone, so the true value, which lies between them, rounds to that word
 * too. When they differ the whole computation runs again with P doubled
 * (128, 256, ... 4096). No host float or double is used anywhere.
 *
 * A function value that is exactly zero (asin 0, acos 1, atan2(0, x > 0),
 * log 1, sin 0, tan 0) can never be bracketed away from zero, so it is
 * given from the identity.
 *
 * The integers are nd500_bigint.h: C has no integer type this wide.
 */

#include "float_math.h"
#include "float_exact.h"
#include "nd500_bigint.h"

#include <stdio.h>
#include <stdlib.h>

#define SIGN 0x8000000000000000ull
#define MAG  0x7FFFFFFFFFFFFFFFull

/* Argument limits in the register pair layout (high word first; an F value
 * is the high word). Unchanged from the earlier code: SIN, COS and TAN above
 * 65536 give IVO and 0; EXP above about 176.75 = 255 ln 2 (12.12) gives IVO
 * and the largest value, below minus that 0. */
#define TRIG_LIMIT 0x4440000000000000ull
static const uint64_t k_exp_limit[2] = { 0x4218605300000000ull, 0x42186052EF911CF3ull };

#define FIRST_PRECISION 128
#define LAST_PRECISION 4096

/* Numbers one attempt may hold at once; the loops reuse theirs. */
#define ARENA_NUMBERS 400

/* A fixed-point value V * 2**-P with |true - V| <= E. */
typedef struct {
    nd500_bi* v;
    nd500_bi* e;
} Fx;

/* One attempt at one precision. */
typedef struct {
    nd500_bi_arena ar;
    int p;
    bool more;          /* the precision was too small somewhere: run again with P doubled */
    nd500_bi* t[6];     /* temporaries for the leaf operations below (none calls another) */
} Cx;

static Fx fx_new(Cx* c) {
    Fx f;
    f.v = nd500_bi_new(&c->ar);
    f.e = nd500_bi_new(&c->ar);
    return f;
}

static void fx_copy(Cx* c, Fx r, Fx a) {
    nd500_bi_copy(&c->ar, r.v, a.v);
    nd500_bi_copy(&c->ar, r.e, a.e);
}

static void fx_add(Cx* c, Fx r, Fx a, Fx b) {
    nd500_bi_add(&c->ar, r.v, a.v, b.v);
    nd500_bi_add(&c->ar, r.e, a.e, b.e);
}

static void fx_sub(Cx* c, Fx r, Fx a, Fx b) {
    nd500_bi_sub(&c->ar, r.v, a.v, b.v);
    nd500_bi_add(&c->ar, r.e, a.e, b.e);
}

static void fx_neg(Cx* c, Fx r, Fx a) {
    nd500_bi_neg(&c->ar, r.v, a.v);
    nd500_bi_copy(&c->ar, r.e, a.e);
}

static void fx_set_one(Cx* c, Fx r) {
    nd500_bi_set_u64(&c->ar, r.v, 1u);
    nd500_bi_shl(&c->ar, r.v, r.v, c->p);
    nd500_bi_set_u64(&c->ar, r.e, 0u);
}

/* a * k for a whole number k: exact apart from the error, which grows by |k|. */
static void fx_mul_int(Cx* c, Fx r, Fx a, int64_t k) {
    nd500_bi_mul_i64(&c->ar, r.v, a.v, k);
    nd500_bi_mul_i64(&c->ar, r.e, a.e, k < 0 ? -k : k);
}

/* a / b: the error of a quotient is (ea + |q| eb) / (|b| - eb), plus 2 for
 * the cut-off. A divisor whose bound reaches zero needs more precision. */
static void fx_div(Cx* c, Fx r, Fx a, Fx b) {
    nd500_bi_arena* ar = &c->ar;
    nd500_bi* bl = c->t[0];
    nd500_bi* q = c->t[1];
    nd500_bi* e = c->t[2];
    nd500_bi* w = c->t[3];
    nd500_bi_abs(ar, bl, b.v);
    nd500_bi_sub(ar, bl, bl, b.e);
    if (nd500_bi_sign(bl) <= 0) {
        c->more = true;
        nd500_bi_set_u64(ar, r.v, 0u);
        nd500_bi_set_u64(ar, r.e, 0u);
        return;
    }
    nd500_bi_shl(ar, q, a.v, c->p);
    nd500_bi_div_trunc(ar, q, q, b.v);
    nd500_bi_shl(ar, e, a.e, c->p);
    nd500_bi_abs(ar, w, q);
    nd500_bi_mul(ar, w, w, b.e);
    nd500_bi_add(ar, e, e, w);
    nd500_bi_div_trunc(ar, e, e, bl);
    nd500_bi_add_i64(ar, e, e, 2);
    nd500_bi_copy(ar, r.v, q);
    nd500_bi_copy(ar, r.e, e);
}

/* Square root of a value >= 0: |sqrt(a) - sqrt(b)| <= |a - b| / sqrt(a - ea),
 * and also <= sqrt|a - b|; the smaller bound is used. */
static void fx_sqrt(Cx* c, Fx r, Fx a) {
    nd500_bi_arena* ar = &c->ar;
    nd500_bi* va = c->t[0];
    nd500_bi* v = c->t[1];
    nd500_bi* e = c->t[2];
    nd500_bi* low = c->t[3];
    nd500_bi* e2 = c->t[4];
    if (nd500_bi_sign(a.v) < 0) {
        nd500_bi_set_u64(ar, va, 0u);
    } else {
        nd500_bi_copy(ar, va, a.v);
    }
    nd500_bi_shl(ar, v, va, c->p);
    nd500_bi_isqrt(ar, v, v);
    nd500_bi_shl(ar, e, a.e, c->p);
    nd500_bi_isqrt(ar, e, e);
    nd500_bi_add_i64(ar, e, e, 2);
    nd500_bi_sub(ar, low, va, a.e);
    if (nd500_bi_sign(low) > 0) {
        nd500_bi_shl(ar, low, low, c->p);
        nd500_bi_isqrt(ar, low, low);
        nd500_bi_shl(ar, e2, a.e, c->p);
        nd500_bi_div_trunc(ar, e2, e2, low);
        nd500_bi_add_i64(ar, e2, e2, 2);
        if (nd500_bi_cmp(e2, e) < 0) {
            nd500_bi_copy(ar, e, e2);
        }
    }
    nd500_bi_copy(ar, r.v, v);
    nd500_bi_copy(ar, r.e, e);
}

/* An exact operand significand * 2**scale as a fixed-point value (cut-off error 1). */
static void fx_exact(Cx* c, Fx r, bool negative, uint64_t sig, int scale) {
    int sh = scale + c->p;
    nd500_bi_set_u64(&c->ar, r.v, sig);
    if (sh >= 0) {
        nd500_bi_shl(&c->ar, r.v, r.v, sh);
    } else {
        nd500_bi_shr_floor(&c->ar, r.v, r.v, -sh);
    }
    if (negative) {
        nd500_bi_neg(&c->ar, r.v, r.v);
    }
    nd500_bi_set_u64(&c->ar, r.e, sh >= 0 ? 0u : 1u);
}

/* fx_exact for a significand already held as a big integer (>= 0). */
static void fx_exact_bi(Cx* c, Fx r, const nd500_bi* sig, int scale) {
    int sh = scale + c->p;
    if (sh >= 0) {
        nd500_bi_shl(&c->ar, r.v, sig, sh);
    } else {
        nd500_bi_shr_floor(&c->ar, r.v, sig, -sh);
    }
    nd500_bi_set_u64(&c->ar, r.e, sh >= 0 ? 0u : 1u);
}

/* The exact quotient (sa * 2**xa) / (sb * 2**xb) of two positive values (error 1). */
static void fx_ratio(Cx* c, Fx r, const nd500_bi* sa, int xa, const nd500_bi* sb, int xb) {
    nd500_bi_arena* ar = &c->ar;
    nd500_bi* n = c->t[0];
    nd500_bi* d = c->t[1];
    int sh = xa - xb + c->p;
    if (sh >= 0) {
        nd500_bi_shl(ar, n, sa, sh);
        nd500_bi_copy(ar, d, sb);
    } else {
        nd500_bi_copy(ar, n, sa);
        nd500_bi_shl(ar, d, sb, -sh);
    }
    nd500_bi_div_trunc(ar, r.v, n, d);
    nd500_bi_set_u64(ar, r.e, 1u);
}

/* atan(1/n) = sum (-1)**k / ((2k+1) n**(2k+1)); each term costs 2 units,
 * the tail is under one term. hyperbolic: atanh(1/n) = sum 1 / ((2k+1)
 * n**(2k+1)), the tail of a series of ratio 1/n**2 under two terms. */
static void fx_atan_inv(Cx* c, Fx r, uint32_t n, bool hyperbolic) {
    nd500_bi_arena* ar = &c->ar;
    nd500_bi* t = c->t[0];
    nd500_bi* s = c->t[1];
    nd500_bi* term = c->t[2];
    uint32_t nn = n * n;
    nd500_bi_set_u64(ar, t, 1u);
    nd500_bi_shl(ar, t, t, c->p);
    nd500_bi_div_u32(ar, t, t, n);
    nd500_bi_copy(ar, s, t);
    uint32_t k = 1;
    int64_t terms = 1;
    bool minus = true;
    while (!nd500_bi_is_zero(t)) {
        nd500_bi_div_u32(ar, t, t, nn);
        k += 2;
        nd500_bi_div_u32(ar, term, t, k);
        if (minus && !hyperbolic) {
            nd500_bi_sub(ar, s, s, term);
        } else {
            nd500_bi_add(ar, s, s, term);
        }
        minus = !minus;
        terms++;
    }
    nd500_bi_copy(ar, r.v, s);
    nd500_bi_set_i64(ar, r.e, hyperbolic ? 2 * terms + 4 : 2 * terms + 2);
}

/* Machin: pi = 16 atan(1/5) - 4 atan(1/239). */
static void fx_pi(Cx* c, Fx r) {
    Fx a = fx_new(c), b = fx_new(c);
    fx_atan_inv(c, a, 5u, false);
    fx_mul_int(c, a, a, 16);
    fx_atan_inv(c, b, 239u, false);
    fx_mul_int(c, b, b, 4);
    fx_sub(c, r, a, b);
}

/* pi / 2**k: the value and its bound shifted, plus one for the cut-off. */
static void fx_pi_over(Cx* c, Fx r, Fx pi, int k) {
    nd500_bi_shr_floor(&c->ar, r.v, pi.v, k);
    nd500_bi_shr_floor(&c->ar, r.e, pi.e, k);
    nd500_bi_add_i64(&c->ar, r.e, r.e, 1);
}

/* ln 2 = 2 atanh(1/3). */
static void fx_ln2(Cx* c, Fx r) {
    fx_atan_inv(c, r, 3u, true);
    fx_mul_int(c, r, r, 2);
}

/* ln 10 = 3 ln 2 + ln 1.25, and ln 1.25 = 2 atanh(1/9). */
static void fx_ln10(Cx* c, Fx r) {
    Fx a = fx_new(c), b = fx_new(c);
    fx_ln2(c, a);
    fx_mul_int(c, a, a, 3);
    fx_atan_inv(c, b, 9u, true);
    fx_mul_int(c, b, b, 2);
    fx_add(c, r, a, b);
}

/* Taylor series of atan for |z| <= 0.42: slope at most 1, so the error is
 * ez plus 3 units a term (multiply, divide) and the alternating tail, under
 * one term. hyperbolic: atanh for |z| <= 1/3, slope 1/(1-z**2) <= 1.125 (2
 * is used), tail under two terms. */
static void fx_atan_series(Cx* c, Fx r, Fx z, bool hyperbolic) {
    nd500_bi_arena* ar = &c->ar;
    nd500_bi* z2 = c->t[0];
    nd500_bi* t = c->t[1];
    nd500_bi* s = c->t[2];
    nd500_bi* term = c->t[3];
    nd500_bi_mul(ar, z2, z.v, z.v);
    nd500_bi_shr_floor(ar, z2, z2, c->p);
    nd500_bi_copy(ar, t, z.v);
    nd500_bi_copy(ar, s, z.v);
    uint32_t k = 1;
    int64_t terms = 1;
    bool minus = true;
    while (!nd500_bi_is_zero(t)) {
        nd500_bi_mul(ar, t, t, z2);
        nd500_bi_shr_trunc(ar, t, t, c->p);
        k += 2;
        nd500_bi_div_u32(ar, term, t, k);
        if (minus && !hyperbolic) {
            nd500_bi_sub(ar, s, s, term);
        } else {
            nd500_bi_add(ar, s, s, term);
        }
        minus = !minus;
        terms++;
    }
    nd500_bi* e = c->t[4];
    nd500_bi_mul_i64(ar, e, z.e, hyperbolic ? 2 : 1);
    nd500_bi_add_i64(ar, e, e, hyperbolic ? 3 * terms + 6 : 3 * terms + 4);
    nd500_bi_copy(ar, r.v, s);
    nd500_bi_copy(ar, r.e, e);
}

/* exp(r) for |r| <= 0.75: slope e**0.75 < 3, tail under two terms. */
static void fx_exp_series(Cx* c, Fx out, Fx r) {
    nd500_bi_arena* ar = &c->ar;
    nd500_bi* t = c->t[0];
    nd500_bi* s = c->t[1];
    nd500_bi_set_u64(ar, t, 1u);
    nd500_bi_shl(ar, t, t, c->p);
    nd500_bi_copy(ar, s, t);
    uint32_t n = 0;
    int64_t terms = 1;
    while (!nd500_bi_is_zero(t)) {
        n++;
        nd500_bi_mul(ar, t, t, r.v);
        nd500_bi_shr_trunc(ar, t, t, c->p);
        nd500_bi_div_u32(ar, t, t, n);
        nd500_bi_add(ar, s, s, t);
        terms++;
    }
    nd500_bi* e = c->t[2];
    nd500_bi_mul_i64(ar, e, r.e, 3);
    nd500_bi_add_i64(ar, e, e, 3 * terms + 6);
    nd500_bi_copy(ar, out.v, s);
    nd500_bi_copy(ar, out.e, e);
}

/* sin(r) and cos(r) for |r| <= 0.8: slope at most 1, alternating tail under one term. */
static void fx_sin_cos_series(Cx* c, Fx r, Fx sin_out, Fx cos_out) {
    nd500_bi_arena* ar = &c->ar;
    nd500_bi* r2 = c->t[0];
    nd500_bi* ts = c->t[1];
    nd500_bi* ss = c->t[2];
    nd500_bi* tc = c->t[3];
    nd500_bi* sc = c->t[4];
    nd500_bi* e = c->t[5];
    nd500_bi_mul(ar, r2, r.v, r.v);
    nd500_bi_shr_floor(ar, r2, r2, c->p);
    nd500_bi_copy(ar, ts, r.v);
    nd500_bi_copy(ar, ss, ts);
    nd500_bi_set_u64(ar, tc, 1u);
    nd500_bi_shl(ar, tc, tc, c->p);
    nd500_bi_copy(ar, sc, tc);
    int64_t n = 0, terms = 1;
    bool minus = true;
    while (!nd500_bi_is_zero(ts) || !nd500_bi_is_zero(tc)) {
        n += 2;
        nd500_bi_mul(ar, tc, tc, r2);
        nd500_bi_shr_floor(ar, tc, tc, c->p);
        nd500_bi_div_u32(ar, tc, tc, (uint32_t)((n - 1) * n));
        nd500_bi_mul(ar, ts, ts, r2);
        nd500_bi_shr_trunc(ar, ts, ts, c->p);
        nd500_bi_div_u32(ar, ts, ts, (uint32_t)(n * (n + 1)));
        if (minus) {
            nd500_bi_sub(ar, sc, sc, tc);
            nd500_bi_sub(ar, ss, ss, ts);
        } else {
            nd500_bi_add(ar, sc, sc, tc);
            nd500_bi_add(ar, ss, ss, ts);
        }
        minus = !minus;
        terms++;
    }
    nd500_bi_add_i64(ar, e, r.e, 3 * terms + 4);
    nd500_bi_copy(ar, sin_out.v, ss);
    nd500_bi_copy(ar, sin_out.e, e);
    nd500_bi_copy(ar, cos_out.v, sc);
    nd500_bi_copy(ar, cos_out.e, e);
}

/* The whole number nearest to a fixed-point value (any nearby whole number
 * is valid for a reduction). */
static int64_t fx_nearest(Cx* c, const nd500_bi* v) {
    nd500_bi* h = c->t[0];
    nd500_bi_set_u64(&c->ar, h, 1u);
    nd500_bi_shl(&c->ar, h, h, c->p - 1);
    nd500_bi_add(&c->ar, h, h, v);
    nd500_bi_shr_floor(&c->ar, h, h, c->p);
    int64_t k = (int64_t)nd500_bi_low_u64(h);   /* |k| is far below 2**63: at most 65536 / (pi/2) or 256 */
    return nd500_bi_sign(h) < 0 ? -k : k;
}

/* atan(z) for 0 <= z <= about 1.01: above tan(pi/8) = 0.4142 it uses
 * atan z = pi/4 + atan((z-1)/(z+1)), whose argument is then within
 * -0.42..0.42. */
static void fx_atan_unit(Cx* c, Fx r, Fx z) {
    nd500_bi* tan_pi8 = c->t[0];
    nd500_bi_set_u64(&c->ar, tan_pi8, 4142u);
    nd500_bi_shl(&c->ar, tan_pi8, tan_pi8, c->p);
    nd500_bi_div_u32(&c->ar, tan_pi8, tan_pi8, 10000u);
    if (nd500_bi_cmp(z.v, tan_pi8) <= 0) {
        fx_atan_series(c, r, z, false);
        return;
    }
    Fx one = fx_new(c), num = fx_new(c), den = fx_new(c), w = fx_new(c), pi = fx_new(c), pi4 = fx_new(c);
    fx_set_one(c, one);
    fx_sub(c, num, z, one);
    fx_add(c, den, z, one);
    fx_div(c, w, num, den);
    fx_pi(c, pi);
    fx_pi_over(c, pi4, pi, 2);
    fx_atan_series(c, w, w, false);
    fx_add(c, r, pi4, w);
}

/* atan2(y, x) of two fixed-point values, the angle of the point (x, y) in
 * -pi..pi: the ratio of the smaller to the larger magnitude, atan of it,
 * then the octant. */
static void fx_atan2(Cx* c, Fx r, Fx y, Fx x) {
    Fx ay = fx_new(c), ax = fx_new(c), t = fx_new(c), pi = fx_new(c), pi2 = fx_new(c);
    bool yneg = nd500_bi_sign(y.v) < 0, xneg = nd500_bi_sign(x.v) < 0;
    nd500_bi_abs(&c->ar, ay.v, y.v);
    nd500_bi_copy(&c->ar, ay.e, y.e);
    nd500_bi_abs(&c->ar, ax.v, x.v);
    nd500_bi_copy(&c->ar, ax.e, x.e);
    fx_pi(c, pi);
    fx_pi_over(c, pi2, pi, 1);
    if (nd500_bi_cmp(ay.v, ax.v) <= 0) {
        fx_div(c, t, ay, ax);
        fx_atan_unit(c, t, t);
    } else {
        fx_div(c, t, ax, ay);
        fx_atan_unit(c, t, t);
        fx_sub(c, t, pi2, t);
    }
    if (xneg) {
        fx_sub(c, t, pi, t);
    }
    if (yneg) {
        fx_neg(c, t, t);
    }
    fx_copy(c, r, t);
}

/* Round n * 2**scale (n signed) by 7.2.7, with the 6.5.1 out-of-range
 * results (float_exact.h nd500_fx_round). Only the top bits and a sticky
 * bit are handed on, which rounds the same. */
static uint64_t round_big(const nd500_bi* n, int scale, bool d, unsigned* exc) {
    if (nd500_bi_is_zero(n)) {
        return 0;
    }
    int keep = (d ? 54 : 22) + 1;
    int len = nd500_bi_bitlen(n);
    int drop = len - (keep + 2);
    uint64_t top;
    bool sticky = false;
    if (drop > 0) {
        sticky = nd500_bi_low_bits_nonzero(n, drop);
        /* the top keep+2 bits of |n| */
        uint64_t v = 0;
        for (int i = len - 1; i >= drop; i--) {
            v = (v << 1) | (nd500_bi_testbit(n, i) ? 1u : 0u);
        }
        top = v;
    } else {
        top = nd500_bi_low_u64(n);
        drop = 0;
    }
    return nd500_fx_round(nd500_bi_sign(n) < 0, nd500_u128_from_u64(top), scale + drop, sticky, d, exc);
}

/* Round V * 2**(extra_scale - P) by 7.2.7 if both ends of its bound round
 * to the same word; otherwise ask for more precision. */
static uint64_t resolve(Cx* c, Fx f, int extra_scale, bool d, unsigned* exc) {
    nd500_bi* lo = c->t[0];
    nd500_bi* hi = c->t[1];
    nd500_bi_sub(&c->ar, lo, f.v, f.e);
    nd500_bi_add(&c->ar, hi, f.v, f.e);
    if (c->more || nd500_bi_arena_failed(&c->ar)
        || nd500_bi_sign(lo) != nd500_bi_sign(hi) || nd500_bi_is_zero(lo)) {
        c->more = true;
        return 0;
    }
    unsigned el = 0, eh = 0;
    uint64_t rl = round_big(lo, extra_scale - c->p, d, &el);
    uint64_t rh = round_big(hi, extra_scale - c->p, d, &eh);
    if (rl != rh || el != eh) {
        c->more = true;
        return 0;
    }
    *exc = el;
    return rl;
}

/* The arguments of one function, for the attempt routine. */
typedef struct {
    int which;           /* sin 0, cos 1, tan 2; acos 1 / asin 0; log base */
    bool d;
    bool neg, neg2;
    uint64_t sig, sig2;
    int scale, scale2;
    int64_t e2;          /* ALOG: x = m * 2**e2 */
    uint64_t m;          /* ALOG: the scaled significand */
    unsigned exc;        /* the exceptions of the settled rounding */
} Args;

typedef uint64_t (*attempt_fn)(Cx* c, Args* a);

/* Run one attempt at P = 128, 256, ... until the result is settled. An
 * operand that is not settled at the last precision stops the emulator:
 * the method has no fallback that could give a plausible wrong value. */
static uint64_t settle(attempt_fn fn, Args* a, const char* name) {
    for (int p = FIRST_PRECISION; p <= LAST_PRECISION; p *= 2) {
        Cx c;
        if (!nd500_bi_arena_init(&c.ar, 2 * p + 2048, ARENA_NUMBERS)) {
            fprintf(stderr, "ND-500 %s: out of memory for the %d bit attempt\n", name, p);
            abort();
        }
        c.p = p;
        c.more = false;
        for (int i = 0; i < 6; i++) {
            c.t[i] = nd500_bi_new(&c.ar);
        }
        uint64_t r = fn(&c, a);
        bool done = !c.more && !nd500_bi_arena_failed(&c.ar);
        nd500_bi_arena_free(&c.ar);
        if (done) {
            return r;
        }
    }
    fprintf(stderr, "ND-500 %s: result not settled at %d bits\n", name, LAST_PRECISION);
    abort();
}

/* ---- the functions ---------------------------------------------------- */

static uint64_t attempt_trig(Cx* c, Args* a) {
    Fx xf = fx_new(c), pi = fx_new(c), hp = fx_new(c), q = fx_new(c), r = fx_new(c);
    Fx s = fx_new(c), co = fx_new(c), v = fx_new(c), sinx = fx_new(c), cosx = fx_new(c);
    fx_exact(c, xf, a->neg, a->sig, a->scale);
    fx_pi(c, pi);
    fx_pi_over(c, hp, pi, 1);
    fx_div(c, q, xf, hp);
    int64_t k = fx_nearest(c, q.v);
    fx_mul_int(c, r, hp, k);
    fx_sub(c, r, xf, r);
    fx_sin_cos_series(c, r, s, co);
    int quadrant = (int)(((k % 4) + 4) % 4);
    switch (quadrant) {
    case 0: fx_copy(c, sinx, s);  fx_copy(c, cosx, co); break;
    case 1: fx_copy(c, sinx, co); fx_neg(c, cosx, s);   break;
    case 2: fx_neg(c, sinx, s);   fx_neg(c, cosx, co);  break;
    default: fx_neg(c, sinx, co); fx_copy(c, cosx, s);  break;
    }
    if (a->which == 0) {
        fx_copy(c, v, sinx);
    } else if (a->which == 1) {
        fx_copy(c, v, cosx);
    } else {
        fx_div(c, v, sinx, cosx);
    }
    unsigned e = 0;
    return resolve(c, v, 0, a->d, &e);
}

static uint64_t sin_cos_tan(uint64_t x, int which, bool d, unsigned* exc) {
    uint64_t pair = d ? x : x << 32;
    if ((pair & MAG) > TRIG_LIMIT) {
        *exc |= ND500_FX_IVO;
        return 0;
    }
    Args a = {0};
    a.which = which;
    a.d = d;
    nd500_fx_decode(x, d, &a.neg, &a.sig, &a.scale);
    if (a.sig == 0 && which != 1) {
        return 0;                          /* sin 0 = tan 0 = 0 */
    }
    return settle(attempt_trig, &a, which == 0 ? "SIN" : which == 1 ? "COS" : "TAN");
}

static uint64_t attempt_asin_acos(Cx* c, Args* a) {
    Fx xf = fx_new(c), om = fx_new(c), root = fx_new(c), v = fx_new(c);
    nd500_bi* num = nd500_bi_new(&c->ar);
    nd500_bi* sq = nd500_bi_new(&c->ar);
    fx_exact(c, xf, a->neg, a->sig, a->scale);
    /* 1 - x**2 from the exact operand: (2**(2s') - sig**2) / 2**(2s'), cut-off error 1. */
    int s2 = -2 * a->scale;
    nd500_bi_set_u64(&c->ar, num, 1u);
    nd500_bi_shl(&c->ar, num, num, s2);
    nd500_bi_set_u64(&c->ar, sq, a->sig);
    nd500_bi_mul(&c->ar, sq, sq, sq);
    nd500_bi_sub(&c->ar, num, num, sq);
    fx_exact_bi(c, om, num, -s2);
    fx_sqrt(c, root, om);
    if (a->which == 1) {
        fx_atan2(c, v, root, xf);
    } else {
        fx_atan2(c, v, xf, root);
    }
    unsigned e = 0;
    return resolve(c, v, 0, a->d, &e);
}

static uint64_t asin_acos(uint64_t x, bool acos, bool d, unsigned* exc) {
    Args a = {0};
    a.which = acos ? 1 : 0;
    a.d = d;
    nd500_fx_decode(x, d, &a.neg, &a.sig, &a.scale);
    if (a.sig == 0 && !acos) {
        return 0;                          /* asin 0 = 0 */
    }
    /* |x| compared with 1 exactly: sig * 2**scale against 1. */
    int cmp;
    if (a.sig == 0) {
        cmp = -1;
    } else if (a.scale >= 0) {
        cmp = 1;
    } else if (-a.scale >= 64) {
        cmp = -1;
    } else {
        uint64_t one = (uint64_t)1 << -a.scale;
        cmp = a.sig < one ? -1 : a.sig > one ? 1 : 0;
    }
    if (cmp > 0) {                         /* 12.6 / 12.8: outside -1..+1, IVO, register 0 */
        *exc |= ND500_FX_IVO;
        return 0;
    }
    if (cmp == 0 && acos && !a.neg) {
        return 0;                          /* acos 1 = 0 */
    }
    return settle(attempt_asin_acos, &a, acos ? "ACOS" : "ASIN");
}

static uint64_t attempt_atan2(Cx* c, Args* a) {
    Fx v = fx_new(c);
    if (a->sig != 0 && a->sig2 != 0) {
        /* The ratio straight from the exact operands, so that a huge or tiny operand loses nothing. */
        nd500_bi* sy = nd500_bi_new(&c->ar);
        nd500_bi* sx = nd500_bi_new(&c->ar);
        nd500_bi* ly = nd500_bi_new(&c->ar);
        nd500_bi* lx = nd500_bi_new(&c->ar);
        nd500_bi_set_u64(&c->ar, sy, a->sig);
        nd500_bi_set_u64(&c->ar, sx, a->sig2);
        int ey = a->scale, ex = a->scale2;
        nd500_bi_shl(&c->ar, ly, sy, ey > ex ? ey - ex : 0);
        nd500_bi_shl(&c->ar, lx, sx, ex > ey ? ex - ey : 0);
        bool y_big = nd500_bi_cmp(ly, lx) > 0;
        Fx pi = fx_new(c), pi2 = fx_new(c), t = fx_new(c);
        fx_pi(c, pi);
        fx_pi_over(c, pi2, pi, 1);
        if (y_big) {
            fx_ratio(c, t, sx, ex, sy, ey);
            fx_atan_unit(c, t, t);
            fx_sub(c, t, pi2, t);
        } else {
            fx_ratio(c, t, sy, ey, sx, ex);
            fx_atan_unit(c, t, t);
        }
        if (a->neg2) {
            fx_sub(c, t, pi, t);
        }
        if (a->neg) {
            fx_neg(c, t, t);
        }
        fx_copy(c, v, t);
    } else {
        Fx yf = fx_new(c), xf = fx_new(c);
        fx_exact(c, yf, a->neg, a->sig, a->scale);
        fx_exact(c, xf, a->neg2, a->sig2, a->scale2);
        fx_atan2(c, v, yf, xf);
    }
    unsigned e = 0;
    return resolve(c, v, 0, a->d, &e);
}

static uint64_t atan2_fd(uint64_t y, uint64_t x, bool d, unsigned* exc) {
    Args a = {0};
    a.d = d;
    nd500_fx_decode(y, d, &a.neg, &a.sig, &a.scale);
    nd500_fx_decode(x, d, &a.neg2, &a.sig2, &a.scale2);
    if (a.sig == 0 && a.sig2 == 0) {       /* both zero: invalid operation, as the earlier code did */
        *exc |= ND500_FX_IVO;
        return 0;
    }
    if (a.sig == 0 && !a.neg2) {
        return 0;                          /* atan2(0, x > 0) = 0 */
    }
    return settle(attempt_atan2, &a, "ATAN2");
}

static uint64_t attempt_exp(Cx* c, Args* a) {
    Fx xf = fx_new(c), ln2 = fx_new(c), q = fx_new(c), r = fx_new(c), v = fx_new(c);
    fx_exact(c, xf, a->neg, a->sig, a->scale);
    fx_ln2(c, ln2);
    fx_div(c, q, xf, ln2);
    int64_t k = fx_nearest(c, q.v);
    fx_mul_int(c, r, ln2, k);
    fx_sub(c, r, xf, r);
    fx_exp_series(c, v, r);
    a->exc = 0;
    return resolve(c, v, (int)k, a->d, &a->exc);
}

static uint64_t exp_fd(uint64_t x, bool d, unsigned* exc) {
    uint64_t pair = d ? x : x << 32;
    if ((pair & MAG) > k_exp_limit[d ? 1 : 0]) {
        if (pair & SIGN) {
            return 0;                      /* 12.12: below -255 ln 2 the result is 0 */
        }
        *exc |= ND500_FX_IVO;
        return d ? MAG : MAG >> 32;
    }
    Args a = {0};
    a.d = d;
    nd500_fx_decode(x, d, &a.neg, &a.sig, &a.scale);
    uint64_t r = settle(attempt_exp, &a, "EXP");
    if (a.exc & ND500_FX_FO) {
        *exc |= ND500_FX_IVO;
        return d ? MAG : MAG >> 32;
    }
    if (a.exc & ND500_FX_FU) {
        return 0;
    }
    return r;
}

static uint64_t attempt_alog(Cx* c, Args* a) {
    int mb1 = a->d ? 55 : 23;
    nd500_bi* m = nd500_bi_new(&c->ar);
    nd500_bi* den = nd500_bi_new(&c->ar);
    nd500_bi* diff = nd500_bi_new(&c->ar);
    nd500_bi* sum = nd500_bi_new(&c->ar);
    Fx z = fx_new(c), lnm = fx_new(c), ln = fx_new(c), l2 = fx_new(c), v = fx_new(c);
    nd500_bi_set_u64(&c->ar, m, a->m);
    nd500_bi_set_u64(&c->ar, den, 1u);
    nd500_bi_shl(&c->ar, den, den, mb1);
    nd500_bi_sub(&c->ar, diff, m, den);
    bool below = nd500_bi_sign(diff) < 0;
    nd500_bi_abs(&c->ar, diff, diff);
    nd500_bi_add(&c->ar, sum, m, den);
    fx_ratio(c, z, diff, 0, sum, 0);
    if (below) {
        fx_neg(c, z, z);
    }
    fx_atan_series(c, lnm, z, true);       /* ln m = 2 atanh((m-1)/(m+1)) */
    fx_mul_int(c, lnm, lnm, 2);
    fx_ln2(c, l2);
    fx_mul_int(c, ln, l2, a->e2);
    fx_add(c, ln, ln, lnm);
    if (a->which == 2) {
        fx_div(c, v, ln, l2);
    } else if (a->which == 10) {
        Fx l10 = fx_new(c);
        fx_ln10(c, l10);
        fx_div(c, v, ln, l10);
    } else {
        fx_copy(c, v, ln);
    }
    unsigned e = 0;
    return resolve(c, v, 0, a->d, &e);
}

static uint64_t alog_fd(uint64_t x, int base, bool d, unsigned* exc) {
    Args a = {0};
    a.which = base;
    a.d = d;
    nd500_fx_decode(x, d, &a.neg, &a.sig, &a.scale);
    if (a.sig == 0 || a.neg) {             /* 12.13-12.15: IVO and -5.8*10**76, all bits set */
        *exc |= ND500_FX_IVO;
        return d ? ~(uint64_t)0 : 0xFFFFFFFFull;
    }
    int mb1 = d ? 55 : 23;                 /* significant bits, hidden bit included */
    int64_t e2 = (int64_t)a.scale + mb1;   /* x = m * 2**e2, 0.5 <= m < 1, m = sig / 2**mb1 */
    if (e2 == 1 && a.sig == ((uint64_t)1 << (mb1 - 1))) {
        return 0;                          /* log 1 = 0 */
    }
    /* m < sqrt(1/2) (2 m**2 < 1): use 2m, so m is in sqrt(1/2)..sqrt(2).
     * 2 sig**2 < 2**(2 mb1) is sig < 2**mb1 / sqrt(2); sig has at most 55
     * bits, so the square is formed in 128 bits. */
    {
        /* sig**2 * 2 and 2**(2 mb1), compared as 128-bit numbers by halves */
        uint64_t s = a.sig;
        uint64_t s_lo = s & 0xFFFFFFFFull, s_hi = s >> 32;
        uint64_t ll = s_lo * s_lo, lh = s_lo * s_hi, hh = s_hi * s_hi;
        uint64_t mid = (ll >> 32) + (lh & 0xFFFFFFFFull) * 2;
        uint64_t lo64 = (ll & 0xFFFFFFFFull) | (mid << 32);
        uint64_t hi64 = hh + (lh >> 32) * 2 + (mid >> 32);
        /* times 2 */
        hi64 = (hi64 << 1) | (lo64 >> 63);
        lo64 <<= 1;
        int sh = 2 * mb1;                  /* 46 or 110 */
        uint64_t r_hi = sh >= 64 ? (uint64_t)1 << (sh - 64) : 0;
        uint64_t r_lo = sh >= 64 ? 0 : (uint64_t)1 << sh;
        bool less = hi64 < r_hi || (hi64 == r_hi && lo64 < r_lo);
        a.m = a.sig;
        if (less) {
            a.m <<= 1;
            e2--;
        }
    }
    a.e2 = e2;
    return settle(attempt_alog, &a, "ALOG");
}

/* ---- the interface ---------------------------------------------------- */

uint64_t nd500_fm_one(bool is_double) { return is_double ? 0x4040000000000000ull : 0x40400000ull; }

/* POLY (12.3). Horner's rule on exact values: sum = c(m); sum = sum * x +
 * c(i) for i = m-1 .. 0, the sum held as n * 2**sum_scale with n a big
 * integer, so no step rounds. 12.3 gives the value and no rounding rule for
 * the steps; owner decision 2026-11-24 for the chapter 12 functions: the true
 * value rounded once by 7.2.7. What 12.3 calls a trap condition "at an
 * intermediate step" exists only in a machine that rounds each step; that is
 * not modelled, so FO and FU come from the final value only.
 *
 * Size: each step adds at most the bits of x's significand (55) and an
 * alignment shift of at most the spread of the scales (about 2 * 311 for
 * operands of exponent field 1 .. 511, plus the growth of the sum's scale by
 * x's scale, at most 311, per step), so count * 448 + 2048 bits always hold
 * the sum. A failed arena stops the emulator: there is no plausible value to
 * fall back to. */
uint64_t nd500_fm_poly(uint64_t x, const uint64_t* coeffs, int count, bool is_double,
                       unsigned* exc) {
    nd500_bi_arena ar;
    if (!nd500_bi_arena_init(&ar, count * 448 + 2048, 8)) {
        fprintf(stderr, "ND-500 POLY: out of memory for degree %d\n", count - 1);
        abort();
    }
    nd500_bi* xv = nd500_bi_new(&ar);
    nd500_bi* sum = nd500_bi_new(&ar);
    nd500_bi* c = nd500_bi_new(&ar);
    nd500_bi* t = nd500_bi_new(&ar);

    bool neg;
    uint64_t sig;
    int x_scale;
    nd500_fx_decode(x, is_double, &neg, &sig, &x_scale);
    nd500_bi_set_u64(&ar, xv, sig);
    if (neg) {
        nd500_bi_neg(&ar, xv, xv);
    }

    int sum_scale = 0;
    for (int i = 0; i < count; i++) {
        int c_scale;
        nd500_fx_decode(coeffs[i], is_double, &neg, &sig, &c_scale);
        nd500_bi_set_u64(&ar, c, sig);
        if (neg) {
            nd500_bi_neg(&ar, c, c);
        }
        if (i > 0) {
            nd500_bi_mul(&ar, sum, sum, xv);
            sum_scale += x_scale;
        }
        if (nd500_bi_is_zero(sum)) {
            nd500_bi_copy(&ar, sum, c);
            sum_scale = c_scale;
        } else if (!nd500_bi_is_zero(c)) {
            int common = sum_scale < c_scale ? sum_scale : c_scale;
            nd500_bi_shl(&ar, sum, sum, sum_scale - common);
            nd500_bi_shl(&ar, t, c, c_scale - common);
            nd500_bi_add(&ar, sum, sum, t);
            sum_scale = common;
        }
    }
    if (nd500_bi_arena_failed(&ar)) {
        fprintf(stderr, "ND-500 POLY: the exact sum did not fit (degree %d)\n", count - 1);
        abort();
    }
    unsigned e = 0;
    uint64_t r = round_big(sum, sum_scale, is_double, &e);
    nd500_bi_arena_free(&ar);
    *exc |= e;
    return r;
}

uint64_t nd500_fm_sin(uint64_t x, bool is_double, unsigned* exc) { return sin_cos_tan(x, 0, is_double, exc); }

uint64_t nd500_fm_cos(uint64_t x, bool is_double, unsigned* exc) { return sin_cos_tan(x, 1, is_double, exc); }

uint64_t nd500_fm_tan(uint64_t x, bool is_double, unsigned* exc) { return sin_cos_tan(x, 2, is_double, exc); }

uint64_t nd500_fm_asin(uint64_t x, bool is_double, unsigned* exc) { return asin_acos(x, false, is_double, exc); }

uint64_t nd500_fm_acos(uint64_t x, bool is_double, unsigned* exc) { return asin_acos(x, true, is_double, exc); }

uint64_t nd500_fm_atan2(uint64_t y, uint64_t x, bool is_double, unsigned* exc) { return atan2_fd(y, x, is_double, exc); }

uint64_t nd500_fm_exp(uint64_t x, bool is_double, unsigned* exc) { return exp_fd(x, is_double, exc); }

uint64_t nd500_fm_alog(uint64_t x, int base, bool is_double, unsigned* exc) { return alog_fd(x, base, is_double, exc); }
