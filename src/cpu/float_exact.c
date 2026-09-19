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

typedef unsigned __int128 u128;

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
    while (v != 0) {
        v >>= 1;
        n++;
    }
    return n;
}

uint64_t nd500_fx_round(bool negative, u128 magnitude, int scale,
                        bool sticky, bool is_double, unsigned* exc) {
    int mb = mantissa_bits(is_double);
    uint64_t sign = negative ? ((uint64_t)1 << (mb + 9)) : 0;
    if (magnitude == 0) {
        return 0;
    }
    int keep = mb + 1;
    int shift = bit_length(magnitude) - keep;
    u128 q;
    bool g = false;
    bool st = sticky;
    if (shift > 0) {
        q = magnitude >> shift;
        g = ((magnitude >> (shift - 1)) & 1) != 0;
        st = st || (magnitude & ((((u128)1) << (shift - 1)) - 1)) != 0;
    } else {
        q = magnitude << -shift;
    }
    if (g && (st || (q & 1) != 0)) {
        q += 1;
        if (bit_length(q) > keep) {
            q >>= 1;
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
    return sign | ((uint64_t)exponent << mb) | ((uint64_t)q & (((uint64_t)1 << mb) - 1));
}

static uint64_t add_signed(uint64_t a, uint64_t b, bool negate_b, bool is_double, unsigned* exc) {
    bool na, nb;
    uint64_t sa, sb;
    int xa, xb;
    nd500_fx_decode(a, is_double, &na, &sa, &xa);
    nd500_fx_decode(b, is_double, &nb, &sb, &xb);
    nb = nb != negate_b;
    if (sa == 0) {
        return sb == 0 ? 0 : nd500_fx_round(nb, sb, xb, false, is_double, exc);
    }
    if (sb == 0) {
        return nd500_fx_round(na, sa, xa, false, is_double, exc);
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
    u128 va = ((u128)sa) << (d + 2);
    u128 vb = ((u128)sb) << 2;
    int scale = xa - d - 2;
    if (sticky) {
        vb = 1;                     /* below every kept bit: only sets St */
    }
    u128 m;
    bool neg;
    if (na == nb) {
        m = va + vb;
        neg = na;
    } else if (va >= vb) {
        m = va - vb;
        neg = na;
    } else {
        m = vb - va;
        neg = nb;
    }
    if (m == 0) {
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
    return nd500_fx_round(na != nb, (u128)sa * sb, xa + xb, false, is_double, exc);
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
    u128 num = ((u128)sa) << k;
    u128 q = num / sb;
    bool sticky = (num % sb) != 0;
    return nd500_fx_round(na != nb, q, xa - xb - k, sticky, is_double, exc);
}

static u128 isqrt128(u128 n, bool* exact) {
    /* Bit-by-bit integer square root: the largest r with r*r <= n. */
    u128 r = 0;
    u128 bit = ((u128)1) << 126;
    while (bit > n) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (n >= r + bit) {
            n -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    *exact = (n == 0);
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
    u128 r = isqrt128(((u128)sa) << k, &exact);
    return nd500_fx_round(false, r, (xa - k) / 2, !exact, is_double, exc);
}
