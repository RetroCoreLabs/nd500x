/*
 * test_float_math_ballpark.c - the ND-500 mathematical functions (ND-05.009.4
 * chapter 12: SQRT 12.4, SIN 12.5, ASIN 12.6, COS 12.7, ACOS 12.8, TAN 12.9,
 * ATAN 12.10, ATAN2 12.11, EXP 12.12, ALOG 12.13, ALOG2 12.14, ALOG10 12.15),
 * F and D, run through the decoder and dispatch table on many inputs and
 * compared with the host C library as a BALLPARK check
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * THE HOST VALUE IS A BALLPARK REFERENCE ONLY, NOT THE SPEC. The manual gives
 * no algorithm and no rounding rule for these functions; the host library can
 * only say whether a result is within a few last-place units of the true
 * value. A difference of one or two last-place units is not a fault.
 *
 * Host types are not the machine's types: a D value has 55 significant bits,
 * a host double 53. The reference here is the x86 long double (64 significant
 * bits) and its libm (sinl, atanl, ...), so it is finer than D; the bound for
 * D still allows for the long double library error. The difference is
 * computed in long double from the exact ND-500 value.
 *
 * A fault (counted as FAIL): more than the bound in last-place units, a wrong
 * sign, Z not equal to "result = 0", S not equal to the result sign bit (EXP:
 * "0 -> S"), or invalid-operand handling unlike the section (IVO and the
 * register value it names).
 *
 * Operands are in memory behind the ABSOLUTE address code 0C4H (ND-05.009.4
 * appendix B).
 */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/nd500_instructions.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/machine/machine_protos.h"

#define MEMORY_SIZE (1 << 20)
#define CODE_ADDR   0x1000u
#define ARG1_ADDR   0x2000u
#define ARG2_ADDR   0x2010u

/* "A few" last-place units, the owner's line between ballpark agreement and a
 * fault (2026-11-23). F TAN of 12345.6777 is 2.27 units off, the largest seen. */
#define F_BOUND 3.0L
#define D_BOUND 4.0L

static int g_passed;
static int g_failed;

enum invalid_result { INV_ZERO, INV_LARGEST, INV_MOST_NEGATIVE };

typedef struct {
    const char* name;
    uint16_t f_op;
    uint16_t d_op;
    int two_args;
    enum invalid_result inv;
} Fn;

/* ND-05.009.4 chapter 12 hex codes. */
static const Fn k_fns[] = {
    { "SQRT", 0xFCD4, 0xFCD8, 0, INV_ZERO },
    { "SIN", 0xFF58, 0xFF84, 0, INV_ZERO },
    { "ASIN", 0xFF5C, 0xFF88, 0, INV_ZERO },
    { "COS", 0xFF60, 0xFF8C, 0, INV_ZERO },
    { "ACOS", 0xFF64, 0xFF90, 0, INV_ZERO },
    { "TAN", 0xFF68, 0xFF94, 0, INV_ZERO },
    { "ATAN", 0xFF6C, 0xFF98, 0, INV_ZERO },
    { "ATAN2", 0xFF70, 0xFF9C, 1, INV_ZERO },
    { "EXP", 0xFF74, 0xFFA0, 0, INV_LARGEST },
    { "ALOG", 0xFF78, 0xFFA4, 0, INV_MOST_NEGATIVE },
    { "ALOG2", 0xFF7C, 0xFFA8, 0, INV_MOST_NEGATIVE },
    { "ALOG10", 0xFF80, 0xFFAC, 0, INV_MOST_NEGATIVE },
};

static const double k_trig[] = {
    0, 1e-30, -1e-30, 1e-8, -1e-8, 1e-4, 0.01, 0.1, -0.1, 0.25, 0.5, -0.5, 0.75, 0.785, 1, -1, 1.2, 1.5,
    1.5707, 1.5708, -1.5708, 2, 2.5, 3, 3.1415, 3.1416, -3.1416, 4, 4.7, 5, 6, 6.28, 6.2832, 7, 10, -10,
    12.5, 100, -100, 1000, 12345.678, 30000, 65535.5, 65536, -65536, 65537, -65537, 1e6, 1e20,
};
static const double k_arc[] = {
    0, 1e-30, -1e-30, 1e-8, -1e-8, 1e-4, -1e-4, 0.01, 0.1, -0.1, 0.125, -0.125, 0.25, 0.4, 0.49, 0.5, -0.5,
    0.5000001, 0.51, -0.51, 0.6, 0.7, 0.707, 0.75, 0.8, 0.9, -0.9, 0.99, 0.999, 0.9999999, -0.9999999,
    1, -1, 1.0000002, -1.0000002, 1.5, -2, 1e10, 3.9e25,
};
static const double k_atan[] = {
    0, 1e-60, 1e-30, -1e-30, 1e-8, 1e-4, 0.01, 0.1, -0.1, 0.125, 0.25, 0.2679, 0.268, 0.4, 0.5, -0.5,
    0.57735, 0.6, 0.75, 0.9, 1, -1, 1.1, 1.5, 1.732, 2, -2, 3.7320, 3.733, 5, 10, -10, 100, 1e4, 1e8,
    -1e8, 1e20, 3.9e25, -1.2e26, 1e70,
};
static const double k_exp[] = {
    0, 1e-30, -1e-30, 1e-10, 1e-5, 0.01, -0.01, 0.1, 0.3465, 0.35, 0.5, -0.5, 0.6931, 0.6932, 1, -1, 2,
    -2, 3, 5, 10, -10, 20, 50, -50, 100, -100, 150, 176, 176.7, -176, -176.7, 176.76, 177, -177, -180,
    200, -300,
};
static const double k_log[] = {
    1e-75, 1e-30, 1e-10, 0.001, 0.1, 0.25, 0.5, 0.7, 0.7071, 0.7072, 0.9, 0.99, 0.999999, 1, 1.000001,
    1.01, 1.1, 1.4142, 1.4143, 2, 2.718281828459045, 3, 4, 8, 10, 100, 1000, 1e10, 1e30, 1e70, 5e76,
    0, -1, -1e-30,
};
static const double k_sqrt[] = {
    0, 1e-75, 1e-30, 1e-10, 0.01, 0.1, 0.25, 0.5, 0.9999, 1, 1.0001, 2, 3, 4, 10, 100, 12345.678, 1e10,
    1e30, 1e70, 5e76, -1, -1e-30, -100,
};
static const double k_atan2[][2] = {
    { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 }, { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 }, { 0.5, 1 }, { 1, 0.5 },
    { -0.5, 1 }, { 1, -0.5 }, { 3, 4 }, { -3, -4 }, { 4, -3 }, { 1e-30, 1 }, { 1, 1e-30 }, { 1e-30, -1 },
    { 2, 3.4641 }, { 0.2679, 1 }, { 1e30, 1e-30 }, { 123.456, -789.012 }, { -0.001, -1000 }, { 5, 5 },
    { -5, 5 }, { 5, -5 }, { 0, 0 },
};

/* A host double as ND-500 D bits (exact: 53 bits fit in 55). */
static uint64_t to_nd_d(double v) {
    if (v == 0) {
        return 0;
    }
    uint64_t bits;
    memcpy(&bits, &v, 8);
    int e2 = (int)((bits >> 52) & 0x7FF) - 1022;      /* v = 0.1mmm x 2^e2 */
    uint64_t m53 = (bits & 0xFFFFFFFFFFFFFULL) | (1ULL << 52);
    return (bits & 0x8000000000000000ULL) | ((uint64_t)(e2 + 256) << 54) | ((m53 << 2) & 0x3FFFFFFFFFFFFFULL);
}

/* The exact value of ND-500 D bits as a long double (55 bits fit in 64). */
static long double nd_value(uint64_t d) {
    int e = (int)((d >> 54) & 0x1FF);
    if (e == 0) {
        return 0;
    }
    long double r = ldexpl((long double)((d & 0x3FFFFFFFFFFFFFULL) | (1ULL << 54)), e - 256 - 55);
    return (d >> 63) ? -r : r;
}

static long double host(const char* name, long double x, long double y) {
    if (!strcmp(name, "SQRT")) return sqrtl(x);
    if (!strcmp(name, "SIN")) return sinl(x);
    if (!strcmp(name, "ASIN")) return asinl(x);
    if (!strcmp(name, "COS")) return cosl(x);
    if (!strcmp(name, "ACOS")) return acosl(x);
    if (!strcmp(name, "TAN")) return tanl(x);
    if (!strcmp(name, "ATAN")) return atanl(x);
    if (!strcmp(name, "ATAN2")) return atan2l(x, y);
    if (!strcmp(name, "EXP")) return expl(x);
    if (!strcmp(name, "ALOG")) return logl(x);
    if (!strcmp(name, "ALOG2")) return log2l(x);
    return log10l(x);
}

/* The invalid-operand test of each section. */
static int is_invalid(const char* name, long double x, long double y) {
    if (!strcmp(name, "SQRT")) return x < 0;
    if (!strcmp(name, "SIN") || !strcmp(name, "COS") || !strcmp(name, "TAN")) return fabsl(x) > 65536.0L;
    if (!strcmp(name, "ASIN") || !strcmp(name, "ACOS")) return fabsl(x) > 1.0L;
    if (!strcmp(name, "ATAN")) return 0;
    if (!strcmp(name, "ATAN2")) return x == 0 && y == 0;
    if (!strcmp(name, "EXP")) return x > 255.0L * logl(2.0L);
    return x <= 0;
}

static void write_value(Nd500Machine* m, uint32_t addr, uint64_t d, int is_double) {
    nd500_bus_write32(m, addr, (uint32_t)(d >> 32));
    if (is_double) {
        nd500_bus_write32(m, addr + 4, (uint32_t)d);
    }
}

static int run_one(const Fn* fn, int is_double, uint64_t a1, uint64_t a2, uint64_t* result, uint32_t* st1) {
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);
    cpu.ST1 = 0;
    write_value(&m, ARG1_ADDR, a1, is_double);
    write_value(&m, ARG2_ADDR, a2, is_double);
    uint16_t op = is_double ? fn->d_op : fn->f_op;
    uint8_t code[12] = { (uint8_t)(op >> 8), (uint8_t)op, 0xC4, 0, 0, (uint8_t)(ARG1_ADDR >> 8), (uint8_t)ARG1_ADDR,
                         0xC4, 0, 0, (uint8_t)(ARG2_ADDR >> 8), (uint8_t)ARG2_ADDR };
    size_t len = fn->two_args ? 12 : 7;
    for (size_t i = 0; i < len; i++) {
        nd500_bus_write8(&m, CODE_ADDR + (uint32_t)i, code[i]);
    }
    cpu.PC = CODE_ADDR;
    Nd500FetchedInstruction fi;
    int ok = 0;
    if (nd500_decode_at(&m, CODE_ADDR, &fi) == 0 && g_instr_exec_table[fi.opcode]) {
        g_instr_exec_table[fi.opcode](&cpu, &fi);
        *result = is_double ? nd500_read_double_register(&cpu, 1) : (uint64_t)nd500_read_float_register(&cpu, 1) << 32;
        *st1 = cpu.ST1;
        ok = 1;
    }
    nd500_machine_free(&m);
    return ok;
}

static void fault(const char* fmt_name, const char* where, const char* what) {
    g_failed++;
    printf("  FAIL %s %s: %s\n", fmt_name, where, what);
}

static void ballpark(const Fn* fn, int is_double) {
    char label[32];
    snprintf(label, sizeof label, "%s %s", is_double ? "D" : "F", fn->name);
    const double* in = k_log;
    size_t n = sizeof k_log / sizeof k_log[0];
    if (!strcmp(fn->name, "SQRT")) { in = k_sqrt; n = sizeof k_sqrt / sizeof k_sqrt[0]; }
    else if (!strcmp(fn->name, "SIN") || !strcmp(fn->name, "COS") || !strcmp(fn->name, "TAN")) { in = k_trig; n = sizeof k_trig / sizeof k_trig[0]; }
    else if (!strcmp(fn->name, "ASIN") || !strcmp(fn->name, "ACOS")) { in = k_arc; n = sizeof k_arc / sizeof k_arc[0]; }
    else if (!strcmp(fn->name, "ATAN")) { in = k_atan; n = sizeof k_atan / sizeof k_atan[0]; }
    else if (!strcmp(fn->name, "EXP")) { in = k_exp; n = sizeof k_exp / sizeof k_exp[0]; }
    if (fn->two_args) { n = sizeof k_atan2 / sizeof k_atan2[0]; }

    long double worst = 0;
    char worst_at[64] = "-";
    for (size_t i = 0; i < n; i++) {
        double xv = fn->two_args ? k_atan2[i][0] : in[i];
        double yv = fn->two_args ? k_atan2[i][1] : 0;
        uint64_t a1 = to_nd_d(xv), a2 = to_nd_d(yv);
        if (!is_double) {                              /* F: cut to the high word, 22 mantissa bits */
            a1 &= 0xFFFFFFFF00000000ULL;
            a2 &= 0xFFFFFFFF00000000ULL;
        }
        long double x = nd_value(a1), y = nd_value(a2);
        char where[64];
        snprintf(where, sizeof where, fn->two_args ? "(%.9Lg, %.9Lg)" : "%.9Lg", x, y);

        uint64_t r = 0;
        uint32_t st = 0;
        if (!run_one(fn, is_double, a1, a2, &r, &st)) {
            fault(label, where, "decode or dispatch failed");
            continue;
        }
        int zero = ((r >> 54) & 0x1FF) == 0;
        int sign = (int)(r >> 63);
        int z = (st & ND500_FLAG_Z) != 0, s = (st & ND500_FLAG_S) != 0, ivo = (st & ND500_FLAG_IVO) != 0;
        if (z != zero) fault(label, where, "Z does not match result = 0");
        if (strcmp(fn->name, "EXP") && strcmp(fn->name, "SQRT") && s != sign) fault(label, where, "S does not match the sign bit");
        if (!strcmp(fn->name, "EXP") && s) fault(label, where, "S set; 12.12 says 0 -> S");

        if (is_invalid(fn->name, x, y)) {
            uint64_t want = fn->inv == INV_LARGEST ? 0x7FFFFFFFFFFFFFFFULL : fn->inv == INV_MOST_NEGATIVE ? 0xFFFFFFFFFFFFFFFFULL : 0;
            if (!is_double) want &= 0xFFFFFFFF00000000ULL;
            if (!ivo) fault(label, where, "invalid by the section but IVO not set");
            if (r != want) fault(label, where, "invalid result is not the value the section names");
            g_passed++;
            continue;
        }
        if (ivo) fault(label, where, "IVO set on a valid operand");
        if (!strcmp(fn->name, "EXP") && x < -255.0L * logl(2.0L)) {
            /* 12.12: "An <argument> value less than -255*ln(2) will give a result value of zero." */
            if (r != 0) fault(label, where, "EXP below -255 ln 2 is not zero (12.12)");
            g_passed++;
            continue;
        }
        long double h = host(fn->name, x, y);
        if (h != 0 && fabsl(h) < ldexpl(1.0L, -257)) {     /* below the ND-500 range: 0 expected */
            if (!zero) fault(label, where, "host value below the ND-500 range but result not zero");
            continue;
        }
        long double ref = zero ? h : nd_value(r);
        int e = zero ? (ilogbl(h) + 1 + 256) : (int)((r >> 54) & 0x1FF);
        long double ulp = ldexpl(1.0L, e - 256 - (is_double ? 55 : 23));
        long double diff = (h == 0 && zero) ? 0 : (nd_value(r) - h) / ulp;
        (void)ref;
        if (h != 0 && !zero && (h < 0) != sign) fault(label, where, "wrong sign");
        if (fabsl(diff) > fabsl(worst)) {
            worst = diff;
            snprintf(worst_at, sizeof worst_at, "%s", where);
        }
        if (fabsl(diff) > (is_double ? D_BOUND : F_BOUND)) {
            char what[128];
            snprintf(what, sizeof what, "%.3Lf last-place units from the host ballpark (nd 0x%016llX, host %.21Lg)",
                     diff, (unsigned long long)r, h);
            fault(label, where, what);
        } else {
            g_passed++;
        }
    }
    printf("%-9s largest difference %8.3Lf ulp at %s\n", label, worst, worst_at);
}

/* D1 ATAN with the short constant 1 (FF98 01): Table 13 (8.12) makes the :S
 * constant the double float 1.0 (CDF), arctan(1.0) = pi/4, so the register
 * holds about pi/4 and Z is clear (12.10 "result = 0 -> Z"). */
static void d_atan_short_constant(void) {
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);
    cpu.ST1 = 0;
    nd500_bus_write8(&m, CODE_ADDR, 0xFF);
    nd500_bus_write8(&m, CODE_ADDR + 1, 0x98);
    nd500_bus_write8(&m, CODE_ADDR + 2, 0x01);
    cpu.PC = CODE_ADDR;
    Nd500FetchedInstruction fi;
    if (nd500_decode_at(&m, CODE_ADDR, &fi) != 0 || !g_instr_exec_table[fi.opcode]) {
        fault("D ATAN", "$1", "decode or dispatch failed");
    } else {
        g_instr_exec_table[fi.opcode](&cpu, &fi);
        uint64_t r = nd500_read_double_register(&cpu, 1);
        long double v = nd_value(r);
        printf("D ATAN $1 (short constant): 0x%016llX = %.21Lg, ST1 0x%X\n", (unsigned long long)r, v, cpu.ST1);
        if (fabsl(v - atanl(1.0L)) > 1e-15L) fault("D ATAN", "$1", "not pi/4");
        else g_passed++;
        if (cpu.ST1 & ND500_FLAG_Z) fault("D ATAN", "$1", "Z set");
    }
    nd500_machine_free(&m);
}

int main(void) {
    printf("ND-500 mathematical functions against the host library (ballpark only)\n");
    d_atan_short_constant();
    for (size_t i = 0; i < sizeof k_fns / sizeof k_fns[0]; i++) {
        ballpark(&k_fns[i], 0);
        ballpark(&k_fns[i], 1);
    }
    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
