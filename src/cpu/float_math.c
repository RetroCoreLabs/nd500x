/**
 * float_math.c - the ND-500 mathematical functions, single and double
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * See float_math.h. Addresses are octal control store addresses in the
 * MICRO-5800-B30 listing; the constants are the long arguments of the
 * *_CONST tables there (loaded into the scratch register file).
 *
 * Every value in this file is held the way the microcode holds it in a
 * register pair (SC5:SC6 and so on): the D bit layout, high word first.
 * An F value is the high word with a zero low word, which is the same
 * number; `d` selects the width each floating operation is rounded to.
 */

#include "float_math.h"
#include "float_exact.h"

#define SIGN 0x8000000000000000ull
#define MAG  0x7FFFFFFFFFFFFFFFull
#define HALF 0x4000000000000000ull     /* 0.5 (BM36) */
#define ONE  0x4040000000000000ull     /* 1.0 (BM36 | BM26) */
#define EXP1 0x0040000000000000ull     /* one unit in the exponent field (BM26) */
#define MANT 0x003FFFFFFFFFFFFFull     /* the mantissa field */
#define HI(v) ((uint32_t)((v) >> 32))
#define LO(v) ((uint32_t)(v))
#define F(c) ((uint64_t)(c) << 32)     /* an F constant in the register pair layout */

typedef uint64_t (*fx_op)(uint64_t, uint64_t, bool, unsigned*);

/* One AAP operation at width d; exceptions inside a function are not reported. */
static uint64_t aap(fx_op op, uint64_t a, uint64_t b, bool d)
{
    unsigned e = 0;
    return d ? op(a, b, true, &e) : op(a >> 32, b >> 32, false, &e) << 32;
}

static uint64_t add(uint64_t a, uint64_t b, bool d) { return aap(nd500_fx_add, a, b, d); }
static uint64_t sub(uint64_t a, uint64_t b, bool d) { return aap(nd500_fx_sub, a, b, d); }
static uint64_t mul(uint64_t a, uint64_t b, bool d) { return aap(nd500_fx_mul, a, b, d); }
static uint64_t quot(uint64_t a, uint64_t b, bool d) { return aap(nd500_fx_div, a, b, d); }

/* POLLYF* @027044, POLLYD* @027054: c[0] x, then add the next coefficient
 * and multiply by x in turn. The loop counter decides whether the routine
 * returns after an add or after a multiply. */
static uint64_t poly_mul(const uint64_t* c, int n, uint64_t x, bool d)
{
    uint64_t p = mul(c[0], x, d);
    for (int i = 1; i < n; i++) {
        p = mul(add(c[i], p, d), x, d);
    }
    return p;
}

static uint64_t poly_add(const uint64_t* c, int n, uint64_t x, bool d)
{
    return add(c[n - 1], poly_mul(c, n - 1, x, d), d);
}

/* POLLYF+ @027050, POLLYD+ @027064: starts with c[0] + x, returns after an add. */
static uint64_t poly_plus(const uint64_t* c, int n, uint64_t x, bool d)
{
    uint64_t p = add(c[0], x, d);
    for (int i = 1; i < n; i++) {
        p = add(c[i], mul(x, p, d), d);
    }
    return p;
}

/* RAPPF @027074, RAPPD @027102: v P/Q + v */
static uint64_t rapp(uint64_t p, uint64_t q, uint64_t v, bool d)
{
    return add(mul(quot(p, q, d), v, d), v, d);
}

/* INTRF_U @020343 (rounded, half away from zero: the ALU adds half a unit
 * at the binary point to the magnitude and truncates) and INTF_U @020333
 * (truncated): the integer part of the F value in the high word of t. Both
 * F and D take it from the high word alone. Returns the integer as an F
 * value in the register pair layout, and the integer itself. */
static uint64_t int_part(uint64_t t, bool rounded, long* n)
{
    bool neg;
    uint64_t sig;
    int scale;
    nd500_fx_decode(HI(t), false, &neg, &sig, &scale);    /* |t| = sig * 2**scale */
    long k = 0;
    if (sig != 0 && scale > -64) {
        if (scale >= 0) {
            k = (long)(sig << scale);
        } else {
            uint64_t half = rounded ? ((uint64_t)1) << (-scale - 1) : 0;
            k = (long)((sig + half) >> -scale);
        }
    }
    *n = neg ? -k : k;
    if (k == 0) {
        return 0;
    }
    unsigned e = 0;
    return nd500_fx_round(neg, (unsigned __int128)k, 0, false, false, &e) << 32;
}

/* The argument reduction constants of one function at one width. */
typedef struct {
    uint64_t inv;      /* 1/C, the multiplier that gives the multiple */
    uint64_t big;      /* the largest magnitude the function accepts */
    uint64_t c;        /* C; for D only the high word is set, so m * c is exact */
    uint64_t c_low;    /* D: the rest of C */
    uint64_t small;    /* below this the reduced argument is its own result */
} Reduction;

/* XREDU_F @026106 and XREDU_D @026144: m = the rounded integer part of
 * x / C (for COS, of x / C + 0.5, less 0.5 again), r = x - m C.
 *
 * F (XREDU_F5 @026120): m C and the subtraction in double precision, then
 * rounded to single by adding the top bit of the low word to the high word.
 *
 * D (XREDU_D6 @026170): x is split into the truncated integer part of its
 * high word and the rest, and r = (rest + (integer - m c)) - m c_low, each
 * step a double operation. */
static uint64_t reduce(uint64_t x, bool cos, const Reduction* k, bool d, long* n)
{
    uint64_t t = mul(k->inv, x, d);
    if (cos) {
        t = add(HALF, t, d);             /* @026110, @026150 */
    }
    uint64_t m = int_part(t, true, n);
    if (*n == 0) {
        return x;
    }
    if (cos) {
        m = sub(m, HALF, false);         /* @026116, @026161: an F operation at both widths */
    }
    if (!d) {
        uint64_t r = sub(x, mul(k->c, m, true), true);
        return (uint64_t)(HI(r) + ((LO(r) >> 31) & 1)) << 32;
    }
    long unused;
    uint64_t whole = int_part(x, false, &unused);
    uint64_t rest = sub(x, whole, true);
    uint64_t r = add(rest, sub(whole, mul(k->c, m, true), true), true);
    return sub(r, mul(k->c_low, m, true), true);
}

/* ------------------------------------------------------------ SIN, COS */

/* SINF_CONST @020061, SIND_CONST @020074 */
static const Reduction k_sin_red[2] = {
    { F(0x3FD17CC2u), F(0x44400000u), 0x40A487ED5110B461ull, 0, F(0x3D5A827Au) },
    { 0x3FD17CC1B727220Bull, 0x4440000000000000ull, 0x40A4880000000000ull, 0xBC0ABBBD2E7B9676ull,
      0x395A827999FCEF33ull },
};
static const uint64_t k_sin_near_pi2[2] = { F(0x40648245u), 0x406487ED4B688CC7ull };
static const uint64_t k_sin_poly_f[4] = { F(0x3B974E2Du), F(0xBD27D911u), F(0x3E84439Fu), F(0xBF955552u) };
static const uint64_t k_sin_poly_d[8] = {
    0x342203FDA64F7E53ull, 0xB62B908370212670ull, 0x381848F1A1AB50C0ull, 0xB9EB99152D7702AEull,
    0x3B9C778E9493C18Cull, 0xBD2806806804F869ull, 0x3E844444444442C0ull, 0xBF95555555555553ull,
};

/* SINF @001354 / COSF @001364 -> SINF_0 @025761; SIND @001356 / COSD -> SIND_0 @026004 */
static uint64_t sin_cos(uint64_t x, bool cos, bool d, unsigned* exc)
{
    const Reduction* k = &k_sin_red[d];
    uint64_t sc13 = cos ? 0 : (x & SIGN);
    uint64_t a = x & MAG;
    if (a > k->big) {                    /* XREDU_F1, XREDU_D1 -> BIGARG -> IVOZRO */
        *exc |= ND500_FX_IVO;
        return 0;
    }
    long n;
    uint64_t r = reduce(a, cos, k, d, &n);
    uint64_t flip = sc13 ^ ((n & 1) ? SIGN : 0);
    uint64_t mag = r & MAG;
    if (mag < k->small) {                /* @025766, @026011 -> SINF_3, SIND_3 */
        /* A zero high word clears the high word only (C,ALU ALU,FZRO). */
        return HI(mag) == 0 ? LO(r) : (r ^ flip);
    }
    if (mag > k_sin_near_pi2[d]) {       /* @025772, @026016: +-1.0 */
        return ((r & SIGN) | ONE) ^ flip;
    }
    uint64_t p = d ? poly_mul(k_sin_poly_d, 8, mul(r, r, d), d)
                   : poly_mul(k_sin_poly_f, 4, mul(r, r, d), d);
    uint64_t rs = r ^ flip;              /* @025777, @026025 */
    return add(mul(p, rs, d), rs, d);    /* FAAP*F+F @027030, DAAP*F+F @027034 */
}

/* ------------------------------------------------------------------ TAN */

/* TANF_CONST @020015, TAND_CONST @020027 */
static const Reduction k_tan_red[2] = {
    { F(0x40117CC2u), F(0x44400000u), 0x406487ED5110B461ull, 0, F(0x3D5A827Au) },
    { 0x40117CC1B727220Bull, 0x4440000000000000ull, 0x4064880000000000ull, 0xBBCABBBD2E7B9676ull,
      0x395A827999FCEF33ull },
};
static const uint64_t k_tan_p_f[1] = { F(0xBF6219DCu) };
static const uint64_t k_tan_q_f[3] = { F(0x3E8F99BBu), F(0xBFEDDBD8u), ONE };
static const uint64_t k_tan_p_d[3] = { 0xBC4AEADCBA8B1C8Full, 0x3E303A0A98EEAB7Bull, 0xBF844AD7953423FEull };
static const uint64_t k_tan_q_d[5] = {
    0x3B02DDD3C1D9E3A4ull, 0xBD51BD9264252105ull, 0x3EE91E7A85F88565ull, 0xBFF77AC11FEF6755ull, ONE,
};

/* TANF @001374 -> TANF_0 @026030; TAND @001376 -> TAND_0 @026054 */
static uint64_t tan_fd(uint64_t x, bool d, unsigned* exc)
{
    const Reduction* k = &k_tan_red[d];
    if ((x & MAG) > k->big) {
        *exc |= ND500_FX_IVO;
        return 0;
    }
    long n;
    uint64_t r = reduce(x, false, k, d, &n);
    bool odd = (n & 1) != 0;
    if ((r & MAG) < k->small) {          /* @026034, @026060 */
        return odd ? quot(ONE | SIGN, r, d) : r;
    }
    uint64_t r2 = mul(r, r, d);
    uint64_t p = d ? poly_mul(k_tan_p_d, 3, r2, d) : poly_mul(k_tan_p_f, 1, r2, d);
    uint64_t num = add(mul(p, r, d), r, d);
    uint64_t den = d ? poly_add(k_tan_q_d, 5, r2, d) : poly_add(k_tan_q_f, 3, r2, d);
    return odd ? quot(den, num ^ SIGN, d) : quot(num, den, d);   /* DIV_32, DIV_64 */
}

/* ---------------------------------------------------------- ASIN, ACOS */

/* ASINF_CONST @020130, ASIND_CONST @020137; PI_LEAST @020170 is the low
 * word of pi/4, and of pi/2 with one more in the exponent field. */
static const uint64_t k_asin_small[2] = { F(0x3D5A827Au), 0x395A827999FCEF33ull };
static const uint64_t k_asin_pi4[2] = { F(0x402487EDu), 0x402487ED5110B461ull };
static const uint64_t k_asin_p_f[2] = { F(0xC0009033u), F(0x40378B36u) };
static const uint64_t k_asin_q_f[2] = { F(0xC0D8C686u), F(0x40D9A878u) };
static const uint64_t k_asin_p_d[5] = {
    0xC0192EF6D7987998ull, 0x4111385D93B0E49Dull, 0xC18F60B2A69B6A7Cull, 0x41B26A9CD82B5229ull,
    0xC16D7956A0E82989ull,
};
static const uint64_t k_asin_q_d[5] = {
    0xC15F4BA1BBE6187Dull, 0x420B79F2596471BEull, 0xC25F773BF15AA11Eull, 0x4268493C407AE10Cull,
    0xC2121B00F8AE1F31ull,
};

/* ASINF @001360 / ACOSF @001370 -> ASINF_1 @026236; ASIND / ACOSD ->
 * ASIND_1 @026311. The offset from ASIN_CON @026302 is added twice
 * (ASINF_10 @026272, ASIND_10 @026360). */
static uint64_t asin_acos(uint64_t x, bool acos, bool d, unsigned* exc)
{
    uint64_t pi4 = k_asin_pi4[d];
    uint64_t s = x & SIGN;
    uint64_t a = x & MAG;
    uint64_t sc11 = (acos ? SIGN : 0) ^ s;
    uint64_t off, v, g;
    if (HI(a) == 0) {                    /* ASINF_6, ASIND_6: twice the offset */
        return acos ? pi4 + EXP1 : 0;
    }
    if (a > HALF) {                      /* ASINF_4, ASIND_4: v = -2 sqrt((1-|x|)/2) */
        off = acos ? (s ? pi4 + EXP1 : 0) : (pi4 | s);
        uint64_t om = sub(ONE, a, d);
        if (HI(om & MAG) == 0) {
            return off == 0 ? 0 : off + EXP1;
        }
        if (om & SIGN) {                 /* |x| > 1: SQRTF, SQRTD -> IVOZRO */
            *exc |= ND500_FX_IVO;
            return 0;
        }
        g = om - EXP1;                   /* @026254, @026336: halve by the exponent */
        unsigned e = 0;
        uint64_t root = d ? nd500_fx_sqrt(g, true, &e) : nd500_fx_sqrt(g >> 32, false, &e) << 32;
        v = ((root + EXP1) | SIGN) ^ sc11;
    } else {
        off = acos ? pi4 : 0;
        g = mul(a, a, d);
        v = a ^ sc11;
        if (a < k_asin_small[d]) {       /* @026245, @026325 */
            return off == 0 ? v : add(add(v, off, d), off, d);
        }
    }
    uint64_t p = d ? poly_mul(k_asin_p_d, 5, g, d) : poly_mul(k_asin_p_f, 2, g, d);
    uint64_t q = d ? poly_plus(k_asin_q_d, 5, g, d) : poly_plus(k_asin_q_f, 2, g, d);
    uint64_t u = rapp(p, q, v, d);
    return off == 0 ? u : add(add(u, off, d), off, d);
}

/* --------------------------------------------------------- ATAN, ATAN2 */

/* ATANF_CONST @020171 and ATANF_ANGLE @026446; ATAND_CONST @020201 and
 * ATAND_ANGLE @026607. Angle entries 16 and 17 are pi/4 and 3 pi/4. */
typedef struct {
    uint64_t sq3m1, twomsq3, sq3, small;
    uint64_t p[4], q[4];
    int np, nq;
    uint64_t angle[18];
} AtanConsts;

static const AtanConsts k_atan[2] = {
    { F(0x401DB3D7u), F(0x3FC49851u), F(0x406ED9ECu), F(0x3D5A827Au),
      { F(0xBF284349u), F(0xBFF8887Bu) }, { F(0x405A666Au) }, 2, 1,
      { 0,              F(0x40030549u), F(0x406487EDu), F(0x40430549u), F(0x40A487EDu), F(0x4093C69Bu),
        F(0x406487EDu), F(0x40830549u), 0,              F(0xC0030549u), F(0xC06487EDu), F(0xC0430549u),
        F(0xC0A487EDu), F(0xC093C69Bu), F(0xC06487EDu), F(0xC0830549u), F(0x402487EDu), F(0x408B65F2u) } },
    { 0x401DB3D742C26554ull, 0x3FC498517A7B3559ull, 0x406ED9EBA16132AAull, 0x395A827999FCEF33ull,
      { 0xC02B35EB66C61EF5ull, 0xC103F4FD7235A98Dull, 0xC15205FEE78AF32Bull, 0xC12D829944187387ull },
      { 0x41383127852B1C42ull, 0x41B72828C8368F5Aull, 0x41D62848102DB692ull, 0x419221F2F31256A6ull }, 4, 4,
      { 0,                     0x40030548E0B5CD96ull, 0x406487ED5110B461ull, 0x40430548E0B5CD96ull,
        0x40A487ED5110B461ull, 0x4093C69B18E340FCull, 0x406487ED5110B461ull, 0x40830548E0B5CD96ull,
        0,                     0xC0030548E0B5CD96ull, 0xC06487ED5110B461ull, 0xC0430548E0B5CD96ull,
        0xC0A487ED5110B461ull, 0xC093C69B18E340FCull, 0xC06487ED5110B461ull, 0xC0830548E0B5CD96ull,
        0x402487ED5110B461ull, 0x408B65F1FCCC8749ull } },
};

/* ATANF_0 @026366 and ATAND_0 @026500, shared by ATAN (y = the operand,
 * x = 1.0) and ATAN2. The table index is 8 (y < 0) + 4 (x < 0) + 2
 * (|x| < |y|, operands swapped) + 1 (reduced by pi/6); entries 0 and 8 add
 * nothing. */
static uint64_t atan2_fd(uint64_t y, uint64_t x, bool d, unsigned* exc)
{
    const AtanConsts* k = &k_atan[d];
    uint64_t ax = x & MAG, ay = y & MAG;
    uint64_t s1 = (x ^ y) & SIGN;
    if (ax == ay) {                      /* ATANF_2, ATAND_2: +-pi/4 or +-3pi/4 */
        if (ax == 0) {
            *exc |= ND500_FX_IVO;
            return 0;
        }
        return k->angle[(x & SIGN) ? 17 : 16] | (y & SIGN);
    }
    unsigned code = ((y & SIGN) ? 8u : 0u) | ((x & SIGN) ? 4u : 0u);
    uint64_t big = ax, small = ay;
    if (ax < ay) {
        code |= 2;
        s1 ^= SIGN;
        big = ay;
        small = ax;
    }
    uint64_t f;
    if (big & MANT) {
        f = quot(small, big, d);          /* ATANF_D32 -> DIV_32, ATAND_D32 -> DIV_64 */
    } else {
        /* ATANF_DIV @026470, ATAND_DIV @026653: a power-of-two divisor is
         * taken off the exponent field of the high word; a negative
         * difference gives 0. */
        uint32_t q = HI(small) - (HI(big) - HI(ONE));
        f = (q & 0x80000000u) ? 0 : ((uint64_t)q << 32) | LO(small);
    }
    if (f > k->twomsq3) {                /* ATANF_7 @026421, ATAND_7 @026545 */
        code |= 1;
        uint64_t t = sub(sub(mul(k->sq3m1, f, d), HALF, d), HALF, d);
        f = quot(add(t, f, d) ^ s1, add(k->sq3, f, d), d);
    } else if (HI(f) != 0) {
        f ^= s1;                         /* @026417, @026542 */
    }
    if ((f & MAG) >= k->small) {         /* ATANF_9 @026441, ATAND_9 @026600 */
        uint64_t g = mul(f, f, d);
        f = rapp(poly_mul(k->p, k->np, g, d), poly_plus(k->q, k->nq, g, d), f, d);
    }
    uint64_t ang = k->angle[code];
    return ang == 0 ? f : add(ang, f, d);
}

/* ------------------------------------------------------------------ EXP */

/* EXPF_CONST @020232, EXPD_CONST @020244 (ln 2 in two parts for D) */
static const Reduction k_exp_red[2] = {
    { F(0x405C551Eu), F(0x42186053u), 0x4018B90BFBE8E7BDull, 0, F(0x3B800000u) },
    { 0x405C551D94AE0BF8ull, 0x42186052EF911CF3ull, 0x4018C00000000000ull, 0xBD2F4041718432A2ull,
      0x323FFFFF82FF2C27ull },
};
static const uint64_t k_exp_p_f[2] = { F(0x3E442984u), F(0x3FC00000u) };
static const uint64_t k_exp_q_f[2] = { F(0x3F265FAEu), HALF };
static const uint64_t k_exp_p_d[3] = { 0x3C454A91BD637071ull, 0x3E71C391BECFDB7Eull, 0x3FBFFFFFFFFFFFFFull };
static const uint64_t k_exp_q_d[3] = { 0x3D80FE65BF78E025ull, 0x3F31C639C50946CBull, HALF };

/* EXPF @001417 -> EXPF_0 @025674, EXPD @001421 -> EXPD_0 @025720:
 * exp(x) = (0.5 + r P / (Q - r P)) * 2**(n+1), the scaling done by adding
 * n+1 to the exponent field (FWRITE_AAPX, DWRITE_X @027455). */
static uint64_t exp_fd(uint64_t x, bool d, unsigned* exc)
{
    const Reduction* k = &k_exp_red[d];
    if ((x & MAG) > k->big) {            /* BIGARG: IVOMAX, or FORD_ZERO */
        if (x & SIGN) {
            return 0;
        }
        *exc |= ND500_FX_IVO;
        return MAG;
    }
    long n;
    uint64_t r = reduce(x, false, k, d, &n);
    uint64_t scale = (uint64_t)(n + 1) << 54;
    if ((r & MAG) < k->small) {          /* @025704, @025731 */
        return HALF + scale;
    }
    uint64_t g = mul(r, r, d);
    uint64_t rp = mul(r, d ? poly_add(k_exp_p_d, 3, g, d) : poly_add(k_exp_p_f, 2, g, d), d);
    uint64_t q = d ? poly_add(k_exp_q_d, 3, g, d) : poly_add(k_exp_q_f, 2, g, d);
    return add(HALF, quot(rp, sub(q, rp, d), d), d) + scale;
}

/* ---------------------------------------------------------------- ALOG */

/* ALOGF_CONST @020272, ALOGD_CONST @020302 */
typedef struct {
    uint64_t sqh;                        /* sqrt(0.5) */
    uint64_t p[3], q[3];
    int n;
    uint64_t c1, c2, log10e, log2e;      /* c1 = 0.693359375 */
} AlogConsts;

static const AlogConsts k_alog[2] = {
    { F(0x401A827Au), { F(0xC006BF1Eu) }, { F(0xC0EA1F9Du) }, 1,
      F(0x4018C000u), F(0xBD2F4041u), F(0x3FEF2DECu), F(0x405C551Eu) },
    { 0x401A827999FCEF32ull,
      { 0xC0251056CD5AF4A3ull, 0x41418928805ABFB3ull, 0xC1C01FFC4ACED669ull },
      { 0xC18756012D9F383Bull, 0x424E020FE85499F8ull, 0xC2A02FFA703641DEull }, 3,
      0x4018C00000000000ull, 0xBD2F4041718432A2ull, 0x3FEF2DEC549B9439ull, 0x405C551D94AE0BF8ull },
};

/* ALOGF @001423 / ALOG2F @001427 / ALOG10F @001433 -> ALOGF_1 @026671;
 * ALOGD / ALOG2D / ALOG10D -> ALOGD_1 @026736: x = f * 2**n with
 * 0.5 <= f < 1, ln x = (n C2 + r) + n C1. */
static uint64_t alog_fd(uint64_t x, int base, bool d, unsigned* exc)
{
    const AlogConsts* k = &k_alog[d];
    if ((HI(x) & 0xFFC00000u) == 0 || (x & SIGN)) {   /* @026674, @026741 MSORZ -> IVOMIN */
        *exc |= ND500_FX_IVO;
        return ~(uint64_t)0;
    }
    long n = (long)((x >> 54) & 0x1FF) - 256;
    uint64_t f = (x & MANT) | HALF;
    uint64_t fm = sub(f, HALF, d);
    uint64_t znum, zden;
    if (f <= k->sqh) {                   /* @026702, @026747 */
        n--;
        znum = fm;
        zden = add(HALF, fm - EXP1, d);
    } else {                             /* ALOGF_5 @026703, ALOGD_5 @026755 */
        znum = sub(fm, HALF, d);
        zden = add(HALF, f - EXP1, d);
    }
    uint64_t z = quot(znum, zden, d);
    uint64_t w = mul(z, z, d);
    uint64_t r = rapp(poly_mul(k->p, k->n, w, d), poly_plus(k->q, k->n, w, d), z, d);
    unsigned e = 0;
    uint64_t nf = (n == 0) ? 0
        : nd500_fx_round(n < 0, (unsigned __int128)(n < 0 ? -n : n), 0, false, false, &e) << 32;
    uint64_t ln = add(add(mul(k->c2, nf, d), r, d), mul(k->c1, nf, d), d);
    if (base == 2) {
        return mul(k->log2e, ln, d);
    }
    if (base == 10) {
        return mul(k->log10e, ln, d);
    }
    return ln;
}

/* ------------------------------------------------------- the interface */

/* Raw bits in (F in the low 32 bits) to the register pair layout, and back. */
static uint64_t in(uint64_t bits, bool d) { return d ? bits : bits << 32; }
static uint64_t out(uint64_t v, bool d) { return d ? v : v >> 32; }

uint64_t nd500_fm_one(bool is_double) { return out(ONE, is_double); }

uint64_t nd500_fm_sin(uint64_t x, bool is_double, unsigned* exc)
{
    return out(sin_cos(in(x, is_double), false, is_double, exc), is_double);
}

uint64_t nd500_fm_cos(uint64_t x, bool is_double, unsigned* exc)
{
    return out(sin_cos(in(x, is_double), true, is_double, exc), is_double);
}

uint64_t nd500_fm_tan(uint64_t x, bool is_double, unsigned* exc)
{
    return out(tan_fd(in(x, is_double), is_double, exc), is_double);
}

uint64_t nd500_fm_asin(uint64_t x, bool is_double, unsigned* exc)
{
    return out(asin_acos(in(x, is_double), false, is_double, exc), is_double);
}

uint64_t nd500_fm_acos(uint64_t x, bool is_double, unsigned* exc)
{
    return out(asin_acos(in(x, is_double), true, is_double, exc), is_double);
}

uint64_t nd500_fm_atan2(uint64_t y, uint64_t x, bool is_double, unsigned* exc)
{
    return out(atan2_fd(in(y, is_double), in(x, is_double), is_double, exc), is_double);
}

uint64_t nd500_fm_exp(uint64_t x, bool is_double, unsigned* exc)
{
    return out(exp_fd(in(x, is_double), is_double, exc), is_double);
}

uint64_t nd500_fm_alog(uint64_t x, int base, bool is_double, unsigned* exc)
{
    return out(alog_fd(in(x, is_double), base, is_double, exc), is_double);
}
