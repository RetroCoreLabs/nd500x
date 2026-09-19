/**
 * float_math.c - the ND-500 single precision mathematical functions
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * See float_math.h. Addresses are octal control store addresses in the
 * MICRO-5800-B30 listing; the constants are the long arguments of the
 * *_CONST tables there (loaded into the scratch register file).
 */

#include "float_math.h"
#include "float_exact.h"

#define F_SIGN 0x80000000u
#define F_MAG  0x7FFFFFFFu
#define F_HALF 0x40000000u     /* 0.5 (BM36) */
#define F_ONE  0x40400000u     /* 1.0 (BM36 | BM26) */
#define F_EXP1 0x00400000u     /* one unit in the exponent field (BM26) */

/* The AAP operations; exceptions inside a function are not reported. */
static uint32_t fadd(uint32_t a, uint32_t b) { unsigned e = 0; return (uint32_t)nd500_fx_add(a, b, false, &e); }
static uint32_t fsub(uint32_t a, uint32_t b) { unsigned e = 0; return (uint32_t)nd500_fx_sub(a, b, false, &e); }
static uint32_t fmul(uint32_t a, uint32_t b) { unsigned e = 0; return (uint32_t)nd500_fx_mul(a, b, false, &e); }
static uint32_t fdiv(uint32_t a, uint32_t b) { unsigned e = 0; return (uint32_t)nd500_fx_div(a, b, false, &e); }

/* INTRF_U @020343: the integer part rounded, half away from zero (the ALU
 * adds half a unit at the binary point to the magnitude and truncates).
 * Returns the F bits of the integer and the integer itself. */
static uint32_t intr(uint32_t t, long* n)
{
    bool neg;
    uint64_t sig;
    int scale;
    nd500_fx_decode(t, false, &neg, &sig, &scale);    /* |t| = sig * 2**scale */
    long k = 0;
    if (sig != 0 && scale > -64) {
        if (scale >= 0) {
            k = (long)(sig << scale);
        } else {
            k = (long)((sig + (((uint64_t)1) << (-scale - 1))) >> -scale);
        }
    }
    *n = neg ? -k : k;
    if (k == 0) {
        return 0;
    }
    unsigned e = 0;
    return (uint32_t)nd500_fx_round(neg, (unsigned __int128)k, 0, false, false, &e);
}

/* XREDU_F5 @026120 .. @026137: r = x - m * C in double precision (C a
 * double constant), then rounded to single by adding the top bit of the
 * low word to the high word. */
static uint32_t reduce(uint32_t x, uint32_t m, uint64_t c)
{
    unsigned e = 0;
    uint64_t prod = nd500_fx_mul(c, (uint64_t)m << 32, true, &e);
    uint64_t r = nd500_fx_sub((uint64_t)x << 32, prod, true, &e);
    return (uint32_t)(r >> 32) + (uint32_t)((r >> 31) & 1);
}

/* ------------------------------------------------------------ SIN, COS */

/* SINF_CONST @020061 */
#define SC_INVPI   0x3FD17CC2u
#define SC_BIG     0x44400000u            /* 65536 */
#define SC_PI      0x40A487ED5110B461ull  /* pi, double */
#define SC_SMALL   0x3D5A827Au
#define SC_NEARPI2 0x40648245u
static const uint32_t k_sin_poly[4] = { 0x3B974E2Du, 0xBD27D911u, 0x3E84439Fu, 0xBF955552u };

/* SINF @001354 / COSF @001364 -> SINF_0 @025761 */
static uint32_t sin_cos(uint32_t x, bool cos, unsigned* exc)
{
    uint32_t sc13 = cos ? 0 : (x & F_SIGN);
    uint32_t a = x & F_MAG;
    if (a > SC_BIG) {                    /* XREDU_F1 -> BIGARG -> IVOZRO */
        *exc |= ND500_FX_IVO;
        return 0;
    }
    uint32_t t = fmul(a, SC_INVPI);
    if (cos) {
        t = fadd(F_HALF, t);             /* @026110 */
    }
    long n;
    uint32_t nb = intr(t, &n);
    uint32_t m = cos ? fsub(nb, F_HALF) : nb;     /* @026116 */
    uint32_t r1 = (n == 0) ? a : reduce(a, m, SC_PI);
    uint32_t odd = (n & 1) ? F_SIGN : 0;
    uint32_t mag = r1 & F_MAG;
    if (mag < SC_SMALL) {                /* @025766 -> SINF_3 */
        return mag == 0 ? 0 : (r1 ^ sc13 ^ odd);
    }
    if (mag > SC_NEARPI2) {              /* @025772: +-1.0 */
        return ((r1 & F_SIGN) | F_ONE) ^ sc13 ^ odd;
    }
    uint32_t rs = r1 ^ sc13 ^ odd;       /* SINF_5, @025777 */
    uint32_t r2 = fmul(r1, r1);
    uint32_t p = fmul(k_sin_poly[0], r2);                 /* POLLYF* */
    for (int i = 1; i < 4; i++) {
        p = fmul(fadd(k_sin_poly[i], p), r2);
    }
    return fadd(fmul(p, rs), rs);        /* FAAP*F+F @027030 */
}

uint32_t nd500_fm_sin(uint32_t x, unsigned* exc) { return sin_cos(x, false, exc); }
uint32_t nd500_fm_cos(uint32_t x, unsigned* exc) { return sin_cos(x, true, exc); }

/* ------------------------------------------------------------------ TAN */

/* TANF_CONST @020015 */
#define TC_TWOPI   0x40117CC2u
#define TC_PI2     0x406487ED5110B461ull  /* pi/2, double */
#define TC_P1      0xBF6219DCu
#define TC_Q1      0x3E8F99BBu
#define TC_Q2      0xBFEDDBD8u

/* TANF @001374 -> TANF_0 @026030 */
uint32_t nd500_fm_tan(uint32_t x, unsigned* exc)
{
    if ((x & F_MAG) > SC_BIG) {
        *exc |= ND500_FX_IVO;
        return 0;
    }
    long n;
    uint32_t nb = intr(fmul(x, TC_TWOPI), &n);
    uint32_t r1 = (n == 0) ? x : reduce(x, nb, TC_PI2);
    bool odd = (n & 1) != 0;
    if ((r1 & F_MAG) < SC_SMALL) {       /* @026034 */
        return odd ? fdiv(F_ONE | F_SIGN, r1) : r1;
    }
    uint32_t r2 = fmul(r1, r1);
    uint32_t num = fadd(fmul(fmul(TC_P1, r2), r1), r1);
    uint32_t den = fadd(F_ONE, fmul(fadd(TC_Q2, fmul(TC_Q1, r2)), r2));
    return odd ? fdiv(den, num ^ F_SIGN) : fdiv(num, den);   /* DIV_32 */
}

/* ---------------------------------------------------------- ASIN, ACOS */

/* ASINF_CONST @020130 */
#define AC_PI4  0x402487EDu
#define AC_PI2  0x406487EDu
#define AC_P2   0xC0009033u
#define AC_P1   0x40378B36u
#define AC_Q1   0xC0D8C686u
#define AC_Q0   0x40D9A878u

/* ASINF @001360 / ACOSF @001370 -> ASINF_1 @026236. The offset from
 * ASIN_CON @026302 is added twice (ASINF_10 @026272). */
static uint32_t asincos(uint32_t x, bool acos, unsigned* exc)
{
    uint32_t s = x & F_SIGN;
    uint32_t a = x & F_MAG;
    uint32_t sc11 = (acos ? F_SIGN : 0) ^ s;
    uint32_t off, v, g;
    if (a == 0) {                        /* ASINF_6: twice the offset */
        return acos ? AC_PI4 + F_EXP1 : 0;
    }
    if (a > F_HALF) {                    /* ASINF_4: v = -2 sqrt((1-|x|)/2) */
        off = acos ? (s ? AC_PI2 : 0) : (AC_PI4 | s);
        uint32_t om = fsub(F_ONE, a);
        if ((om & F_MAG) == 0) {
            return off == 0 ? 0 : off + F_EXP1;
        }
        if (om & F_SIGN) {               /* |x| > 1: SQRTF -> IVOZRO */
            *exc |= ND500_FX_IVO;
            return 0;
        }
        g = om - F_EXP1;                 /* @026254: halve by the exponent */
        unsigned e = 0;
        uint32_t root = (uint32_t)nd500_fx_sqrt(g, false, &e);
        v = ((root + F_EXP1) | F_SIGN) ^ sc11;
    } else {
        off = acos ? AC_PI4 : 0;
        g = fmul(a, a);
        v = a ^ sc11;
        if (a < SC_SMALL) {              /* @026245 */
            return off == 0 ? v : fadd(fadd(v, off), off);
        }
    }
    uint32_t p = fmul(fadd(AC_P1, fmul(AC_P2, g)), g);
    uint32_t q = fadd(AC_Q0, fmul(fadd(AC_Q1, g), g));
    uint32_t u = fadd(fmul(fdiv(p, q), v), v);
    return off == 0 ? u : fadd(fadd(u, off), off);
}

uint32_t nd500_fm_asin(uint32_t x, unsigned* exc) { return asincos(x, false, exc); }
uint32_t nd500_fm_acos(uint32_t x, unsigned* exc) { return asincos(x, true, exc); }

/* --------------------------------------------------------- ATAN, ATAN2 */

/* ATANF_CONST @020171 and the ATANF_ANGLE table @026446 */
#define TA_SQ3M1   0x401DB3D7u
#define TA_TWOMSQ3 0x3FC49851u
#define TA_SQ3     0x406ED9ECu
#define TA_P1      0xBF284349u
#define TA_P0      0xBFF8887Bu
#define TA_Q0      0x405A666Au
#define TA_PI4     0x402487EDu
#define TA_PI34    0x408B65F2u
static const uint32_t k_atan_angle[16] = {
    0,           0x40030549u, 0x406487EDu, 0x40430549u, 0x40A487EDu, 0x4093C69Bu, 0x406487EDu, 0x40830549u,
    0,           0xC0030549u, 0xC06487EDu, 0xC0430549u, 0xC0A487EDu, 0xC093C69Bu, 0xC06487EDu, 0xC0830549u,
};

/* ATANF_0 @026366, shared by ATAN (y = the operand, x = 1.0) and ATAN2.
 * The table index is 8 (y < 0) + 4 (x < 0) + 2 (|x| < |y|, operands
 * swapped) + 1 (reduced by pi/6); entries 0 and 8 add nothing. */
uint32_t nd500_fm_atan2(uint32_t y, uint32_t x, unsigned* exc)
{
    uint32_t ax = x & F_MAG, ay = y & F_MAG;
    uint32_t s1 = (x ^ y) & F_SIGN;
    if (ax == ay) {                      /* ATANF_2: +-pi/4 or +-3pi/4 */
        if (ax == 0) {
            *exc |= ND500_FX_IVO;
            return 0;
        }
        return ((x & F_SIGN) ? TA_PI34 : TA_PI4) | (y & F_SIGN);
    }
    unsigned code = ((y & F_SIGN) ? 8u : 0u) | ((x & F_SIGN) ? 4u : 0u);
    uint32_t big = ax, small = ay;
    if (ax < ay) {
        code |= 2;
        s1 ^= F_SIGN;
        big = ay;
        small = ax;
    }
    uint32_t f;
    if (big & 0x3FFFFFu) {
        f = fdiv(small, big);            /* ATANF_D32 -> DIV_32 */
    } else {
        /* ATANF_DIV @026470: a power-of-two divisor is taken off the
         * exponent field; a negative difference gives 0. */
        uint32_t q = small - (big - F_ONE);
        f = (q & F_SIGN) ? 0 : q;
    }
    if (f > TA_TWOMSQ3) {                /* ATANF_7 @026421 */
        code |= 1;
        uint32_t t = fsub(fsub(fmul(TA_SQ3M1, f), F_HALF), F_HALF);
        f = fdiv(fadd(t, f) ^ s1, fadd(TA_SQ3, f));
    } else if (f != 0) {
        f ^= s1;                         /* @026417 */
    }
    if ((f & F_MAG) >= SC_SMALL) {       /* ATANF_9 @026441 */
        uint32_t g = fmul(f, f);
        uint32_t p = fmul(fadd(TA_P0, fmul(TA_P1, g)), g);
        uint32_t r = fdiv(p, fadd(TA_Q0, g));
        f = fadd(fmul(r, f), f);
    }
    uint32_t ang = k_atan_angle[code];
    return ang == 0 ? f : fadd(ang, f);
}

/* ------------------------------------------------------------------ EXP */

/* EXPF_CONST @020232 */
#define EC_INVLN2 0x405C551Eu
#define EC_BIG    0x42186053u             /* 176.7525 */
#define EC_LN2    0x4018B90BFBE8E7BDull   /* ln 2, double */
#define EC_SMALL  0x3B800000u
#define EC_P1     0x3E442984u
#define EC_P0     0x3FC00000u
#define EC_Q1     0x3F265FAEu

/* EXPF @001417 -> EXPF_0 @025674: exp(x) = (0.5 + r P / (Q - r P)) * 2**(n+1),
 * the scaling done by adding n+1 to the exponent field (FWRITE_AAPX). */
uint32_t nd500_fm_exp(uint32_t x, unsigned* exc)
{
    if ((x & F_MAG) > EC_BIG) {          /* BIGARG: IVOMAX, or FORD_ZERO */
        if (x & F_SIGN) {
            return 0;
        }
        *exc |= ND500_FX_IVO;
        return 0x7FFFFFFFu;
    }
    long n;
    uint32_t nb = intr(fmul(x, EC_INVLN2), &n);
    uint32_t r1 = (n == 0) ? x : reduce(x, nb, EC_LN2);
    if ((r1 & F_MAG) < EC_SMALL) {       /* @025704 */
        return F_HALF + ((uint32_t)(n + 1) << 22);
    }
    uint32_t g = fmul(r1, r1);
    uint32_t rp = fmul(r1, fadd(EC_P0, fmul(EC_P1, g)));
    uint32_t q = fadd(F_HALF, fmul(EC_Q1, g));
    uint32_t r = fadd(F_HALF, fdiv(rp, fsub(q, rp)));
    return r + ((uint32_t)(n + 1) << 22);
}

/* ---------------------------------------------------------------- ALOG */

/* ALOGF_CONST @020272 */
#define LC_SQH    0x401A827Au             /* sqrt(0.5) */
#define LC_P0     0xC006BF1Eu
#define LC_Q0     0xC0EA1F9Du
#define LC_C1     0x4018C000u             /* 0.693359375 */
#define LC_C2     0xBD2F4041u
#define LC_LOG10E 0x3FEF2DECu
#define LC_LOG2E  0x405C551Eu

/* ALOGF @001423 / ALOG2F @001427 / ALOG10F @001433 -> ALOGF_1 @026671:
 * x = f * 2**n with 0.5 <= f < 1, ln x = (n C2 + r) + n C1. */
uint32_t nd500_fm_alog(uint32_t x, int base, unsigned* exc)
{
    if ((x & 0xFFC00000u) == 0 || (x & F_SIGN)) {   /* @026674 MSORZ -> IVOMIN */
        *exc |= ND500_FX_IVO;
        return 0xFFFFFFFFu;
    }
    long n = (long)((x >> 22) & 0x1FF) - 256;
    uint32_t f = (x & 0x3FFFFFu) | F_HALF;
    uint32_t fm = fsub(f, F_HALF);
    uint32_t znum, zden;
    if (f <= LC_SQH) {                   /* @026702 */
        n--;
        znum = fm;
        zden = fadd(F_HALF, fm - F_EXP1);
    } else {                             /* ALOGF_5 @026703 */
        znum = fsub(fm, F_HALF);
        zden = fadd(F_HALF, f - F_EXP1);
    }
    uint32_t z = fdiv(znum, zden);
    uint32_t w = fmul(z, z);
    uint32_t r = fadd(fmul(fdiv(fmul(LC_P0, w), fadd(LC_Q0, w)), z), z);
    unsigned e = 0;
    uint32_t nf = (n == 0) ? 0
        : (uint32_t)nd500_fx_round(n < 0, (unsigned __int128)(n < 0 ? -n : n), 0, false, false, &e);
    uint32_t ln = fadd(fadd(fmul(LC_C2, nf), r), fmul(LC_C1, nf));
    if (base == 2) {
        return fmul(LC_LOG2E, ln);
    }
    if (base == 10) {
        return fmul(LC_LOG10E, ln);
    }
    return ln;
}
