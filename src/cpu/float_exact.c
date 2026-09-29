/**
 * float_exact.c - exact ND-500 floating point arithmetic
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * See float_exact.h for the rules taken from the manual.
 */

#include "float_exact.h"

typedef nd500_u128 u128;

/* 128-bit operations. With a native unsigned __int128 each one is the plain
 * operator; otherwise they work on the two 64-bit halves. Shift counts are
 * 0..127. */
#if defined(__SIZEOF_INT128__) && !defined(ND500_U128_PORTABLE)
static inline u128 u_from(uint64_t v) { return v; }
static inline u128 u_shl(u128 v, int n) { return v << n; }
static inline u128 u_shr(u128 v, int n) { return v >> n; }
static inline u128 u_add(u128 a, u128 b) { return a + b; }
static inline u128 u_sub(u128 a, u128 b) { return a - b; }
static inline bool u_ge(u128 a, u128 b) { return a >= b; }
static inline bool u_gt(u128 a, u128 b) { return a > b; }
static inline bool u_is_zero(u128 v) { return v == 0; }
static inline uint64_t u_low64(u128 v) { return (uint64_t)v; }
static inline u128 u_mul64(uint64_t a, uint64_t b) { return (u128)a * b; }
static inline u128 u_divmod64(u128 num, uint64_t d, uint64_t* rem) {
    *rem = (uint64_t)(num % d);
    return num / d;
}
#else
static inline u128 u_from(uint64_t v) { return nd500_u128_from_u64(v); }
static inline u128 u_shl(u128 v, int n) {
    u128 r;
    if (n == 0) {
        return v;
    }
    if (n >= 64) {
        r.hi = v.lo << (n - 64);
        r.lo = 0;
    } else {
        r.hi = (v.hi << n) | (v.lo >> (64 - n));
        r.lo = v.lo << n;
    }
    return r;
}
static inline u128 u_shr(u128 v, int n) {
    u128 r;
    if (n == 0) {
        return v;
    }
    if (n >= 64) {
        r.lo = v.hi >> (n - 64);
        r.hi = 0;
    } else {
        r.lo = (v.lo >> n) | (v.hi << (64 - n));
        r.hi = v.hi >> n;
    }
    return r;
}
static inline u128 u_add(u128 a, u128 b) {
    u128 r;
    r.lo = a.lo + b.lo;
    r.hi = a.hi + b.hi + (r.lo < a.lo ? 1u : 0u);
    return r;
}
static inline u128 u_sub(u128 a, u128 b) {
    u128 r;
    r.lo = a.lo - b.lo;
    r.hi = a.hi - b.hi - (a.lo < b.lo ? 1u : 0u);
    return r;
}
static inline bool u_ge(u128 a, u128 b) {
    return a.hi != b.hi ? a.hi > b.hi : a.lo >= b.lo;
}
static inline bool u_gt(u128 a, u128 b) {
    return a.hi != b.hi ? a.hi > b.hi : a.lo > b.lo;
}
static inline bool u_is_zero(u128 v) { return v.hi == 0 && v.lo == 0; }
static inline uint64_t u_low64(u128 v) { return v.lo; }
static inline u128 u_mul64(uint64_t a, uint64_t b) {
    /* Schoolbook multiply on 32-bit halves. */
    uint64_t al = a & 0xFFFFFFFFu, ah = a >> 32;
    uint64_t bl = b & 0xFFFFFFFFu, bh = b >> 32;
    uint64_t ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    uint64_t mid = (ll >> 32) + (lh & 0xFFFFFFFFu) + (hl & 0xFFFFFFFFu);
    u128 r;
    r.lo = (ll & 0xFFFFFFFFu) | (mid << 32);
    r.hi = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
    return r;
}
static inline u128 u_divmod64(u128 num, uint64_t d, uint64_t* rem) {
    /* Restoring long division, one quotient bit per step. */
    u128 q = u_from(0);
    u128 r = u_from(0);
    u128 dd = u_from(d);
    int i;
    for (i = 127; i >= 0; i--) {
        r = u_shl(r, 1);
        r.lo |= u_low64(u_shr(num, i)) & 1u;
        if (u_ge(r, dd)) {
            r = u_sub(r, dd);
            if (i >= 64) {
                q.hi |= (uint64_t)1 << (i - 64);
            } else {
                q.lo |= (uint64_t)1 << i;
            }
        }
    }
    *rem = r.lo;
    return q;
}
#endif

/* Bit n of v (0 or 1), and whether any of the n lowest bits is set. */
static inline bool u_bit(u128 v, int n) { return (u_low64(u_shr(v, n)) & 1u) != 0; }
static inline bool u_low_bits_set(u128 v, int n) {
    return n > 0 && !u_is_zero(u_shl(v, 128 - n));
}

static int mantissa_bits(bool is_double) { return is_double ? 54 : 22; }

void nd500_fx_decode(uint64_t bits, bool is_double, bool* negative,
                     uint64_t* significand, int* scale) {
    int mb = mantissa_bits(is_double);
    *negative = ((bits >> (mb + 9)) & 1) != 0;
    unsigned exponent = (unsigned)((bits >> mb) & 0x1FF);
    if (exponent == 0) {
        *significand = 0;
        *scale = 0;
        return;
    }
    *significand = ((uint64_t)1 << mb) | (bits & (((uint64_t)1 << mb) - 1));
    *scale = (int)exponent - 256 - (mb + 1);
}

static int bit_length(u128 v) {
    int n = 0;
    while (!u_is_zero(v)) {
        v = u_shr(v, 1);
        n++;
    }
    return n;
}

uint64_t nd500_fx_round(bool negative, u128 magnitude, int scale,
                        bool sticky, bool is_double, unsigned* exc) {
    int mb = mantissa_bits(is_double);
    uint64_t sign = negative ? ((uint64_t)1 << (mb + 9)) : 0;
    if (u_is_zero(magnitude)) {
        return 0;
    }
    int keep = mb + 1;
    int shift = bit_length(magnitude) - keep;
    u128 q;
    bool g = false;
    bool st = sticky;
    if (shift > 0) {
        q = u_shr(magnitude, shift);
        g = u_bit(magnitude, shift - 1);
        st = st || u_low_bits_set(magnitude, shift - 1);
    } else {
        q = u_shl(magnitude, -shift);
    }
    if (g && (st || u_bit(q, 0))) {
        q = u_add(q, u_from(1));
        if (bit_length(q) > keep) {
            q = u_shr(q, 1);
            shift++;
        }
    }
    int exponent = scale + shift + keep + 256;
    if (exponent > 511) {
        *exc |= ND500_FX_FO;
        return sign | ((uint64_t)511 << mb) | (((uint64_t)1 << mb) - 1);
    }
    if (exponent < 1) {
        *exc |= ND500_FX_FU;
        return sign;
    }
    return sign | ((uint64_t)exponent << mb) | (u_low64(q) & (((uint64_t)1 << mb) - 1));
}

static uint64_t add_signed(uint64_t a, uint64_t b, bool negate_b, bool is_double, unsigned* exc) {
    bool na, nb;
    uint64_t sa, sb;
    int xa, xb;
    nd500_fx_decode(a, is_double, &na, &sa, &xa);
    nd500_fx_decode(b, is_double, &nb, &sb, &xb);
    nb = nb != negate_b;
    if (sa == 0) {
        return sb == 0 ? 0 : nd500_fx_round(nb, u_from(sb), xb, false, is_double, exc);
    }
    if (sb == 0) {
        return nd500_fx_round(na, u_from(sa), xa, false, is_double, exc);
    }
    /* Put the larger exponent in a. Aligning b more than 64 bits below a
     * cannot change anything but the sticky bit, so the shift is capped
     * there and the lost bits are kept as sticky. */
    if (xb > xa) {
        bool tn = na; na = nb; nb = tn;
        uint64_t ts = sa; sa = sb; sb = ts;
        int tx = xa; xa = xb; xb = tx;
    }
    int d = xa - xb;
    bool sticky = false;
    if (d > 64) {
        sticky = true;              /* b is nonzero and entirely below a */
        sb = 0;
        d = 64;
    }
    /* Work 2 bits below b so a lost b still leaves room for G and St. */
    u128 va = u_shl(u_from(sa), d + 2);
    u128 vb = u_shl(u_from(sb), 2);
    int scale = xa - d - 2;
    if (sticky) {
        vb = u_from(1);             /* below every kept bit: only sets St */
    }
    u128 m;
    bool neg;
    if (na == nb) {
        m = u_add(va, vb);
        neg = na;
    } else if (u_ge(va, vb)) {
        m = u_sub(va, vb);
        neg = na;
    } else {
        m = u_sub(vb, va);
        neg = nb;
    }
    if (u_is_zero(m)) {
        return 0;
    }
    return nd500_fx_round(neg, m, scale, false, is_double, exc);
}

uint64_t nd500_fx_add(uint64_t a, uint64_t b, bool is_double, unsigned* exc) {
    return add_signed(a, b, false, is_double, exc);
}

uint64_t nd500_fx_sub(uint64_t a, uint64_t b, bool is_double, unsigned* exc) {
    return add_signed(a, b, true, is_double, exc);
}

uint64_t nd500_fx_mul(uint64_t a, uint64_t b, bool is_double, unsigned* exc) {
    bool na, nb;
    uint64_t sa, sb;
    int xa, xb;
    nd500_fx_decode(a, is_double, &na, &sa, &xa);
    nd500_fx_decode(b, is_double, &nb, &sb, &xb);
    if (sa == 0 || sb == 0) {
        return 0;
    }
    return nd500_fx_round(na != nb, u_mul64(sa, sb), xa + xb, false, is_double, exc);
}

uint64_t nd500_fx_div(uint64_t a, uint64_t b, bool is_double, unsigned* exc) {
    bool na, nb;
    uint64_t sa, sb;
    int xa, xb;
    nd500_fx_decode(a, is_double, &na, &sa, &xa);
    nd500_fx_decode(b, is_double, &nb, &sb, &xb);
    if (sb == 0) {
        *exc |= ND500_FX_DZ;
        return a;
    }
    if (sa == 0) {
        return 0;
    }
    /* sa/sb lies in (1/2, 2); k extra bits give a quotient of at least
     * keep + 2 bits, and the remainder is the rest of St. */
    int k = mantissa_bits(is_double) + 4;
    uint64_t rem;
    u128 num = u_shl(u_from(sa), k);
    u128 q = u_divmod64(num, sb, &rem);
    bool sticky = rem != 0;
    return nd500_fx_round(na != nb, q, xa - xb - k, sticky, is_double, exc);
}

static u128 isqrt128(u128 n, bool* exact) {
    /* Bit-by-bit integer square root: the largest r with r*r <= n. */
    u128 r = u_from(0);
    u128 bit = u_shl(u_from(1), 126);
    while (u_gt(bit, n)) {
        bit = u_shr(bit, 2);
    }
    while (!u_is_zero(bit)) {
        u128 t = u_add(r, bit);
        if (u_ge(n, t)) {
            n = u_sub(n, t);
            r = u_add(u_shr(r, 1), bit);
        } else {
            r = u_shr(r, 1);
        }
        bit = u_shr(bit, 2);
    }
    *exact = u_is_zero(n);
    return r;
}

uint64_t nd500_fx_sqrt(uint64_t a, bool is_double, unsigned* exc) {
    bool neg;
    uint64_t sa;
    int xa;
    nd500_fx_decode(a, is_double, &neg, &sa, &xa);
    if (sa == 0) {
        return 0;
    }
    if (neg) {
        *exc |= ND500_FX_IVO;
        return 0;
    }
    /* value = sa * 2**xa. Scale sa up by an even-adjusted k so the root has
     * at least keep + 2 bits; the remainder of the root is the rest of St. */
    int k = mantissa_bits(is_double) + 6;
    if ((xa - k) & 1) {
        k++;
    }
    bool exact;
    u128 r = isqrt128(u_shl(u_from(sa), k), &exact);
    return nd500_fx_round(false, r, (xa - k) / 2, !exact, is_double, exc);
}
