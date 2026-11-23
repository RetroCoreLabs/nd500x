/*
 * test_fconr_axi.c - D FCONR and F AXI end in the FPU's double-to-single
 * convert with rounding, as ND-05.009.4 7.2.7 and the ND-500 course NEC-01
 * page 69 say
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * The rule (7.2.7): L the last kept mantissa bit, G the next bit, St the OR
 * of all bits below G; "if G=1 and (St=1 or L=1) then add one to the least
 * significant bit of mantissa". NEC-01 page 69: "Rounding is performed in
 * accordance with the IEEE proposed standard for Floating Point Arithmetic
 * concerning addition, subtraction and conversion instructions."
 *
 * Layout (7.2.5/7.2.6): single = sign(1) exponent(9) mantissa(22), double =
 * sign(1) exponent(9) mantissa(54); the same exponent, so a double turns into
 * a single by keeping the top 22 of its 54 mantissa bits: L is bit 32, G is
 * bit 31, St is bits 0-30. The reference below works on those bit fields
 * directly; it shares no code with src/cpu/float_exact.c.
 *
 * The instructions are run through the decoder and the dispatch table, with
 * operands in memory (absolute addresses), so the test covers the code path
 * a guest program takes.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/nd500_instructions.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/machine/machine_protos.h"

#define MEMORY_SIZE (1 << 20)
#define CODE_ADDR   0x1000u
#define SRC_ADDR    0x2000u
#define DST_ADDR    0x2100u
#define INT_ADDR    0x2200u

static int g_passed;
static int g_failed;

static void check_u64(uint64_t got, uint64_t want, const char* what) {
    if (got == want) {
        g_passed++;
    } else {
        g_failed++;
        printf("  FAIL: %s: got 0x%016llX, want 0x%016llX\n", what,
               (unsigned long long)got, (unsigned long long)want);
    }
}

static void check_true(int cond, const char* what) {
    if (cond) {
        g_passed++;
    } else {
        g_failed++;
        printf("  FAIL: %s\n", what);
    }
}

/* 7.2.7 on the bit fields: double -> single, no overflow handling (the
 * cases that overflow are checked by hand). */
static uint32_t ref_d_to_f(uint64_t d) {
    if (((d >> 54) & 0x1FFu) == 0) {
        return 0;                                    /* 7.2.6: exactly zero */
    }
    uint32_t sign = (uint32_t)(d >> 63);
    uint32_t exponent = (uint32_t)((d >> 54) & 0x1FFu);
    uint64_t mant = d & ((1ULL << 54) - 1);
    uint32_t kept = (uint32_t)(mant >> 32);          /* 22 bits */
    unsigned l = (unsigned)(kept & 1u);
    unsigned g = (unsigned)((mant >> 31) & 1u);
    unsigned st = (mant & 0x7FFFFFFFULL) != 0;
    if (g && (st || l)) {
        kept++;
        if (kept == (1u << 22)) {                    /* the mantissa carried out */
            kept = 0;
            exponent++;
        }
    }
    return (sign << 31) | (exponent << 22) | kept;
}

static void setup(Nd500Machine* m, Nd500Cpu* cpu) {
    nd500_machine_init(m, MEMORY_SIZE);
    nd500_cpu_init(cpu, m);
    nd500_cpu_reset(cpu);
    cpu->ST1 = 0;
}

static int run(Nd500Machine* m, Nd500Cpu* cpu, const uint8_t* code, size_t len) {
    for (size_t i = 0; i < len; i++) {
        nd500_bus_write8(m, CODE_ADDR + (uint32_t)i, code[i]);
    }
    cpu->PC = CODE_ADDR;
    Nd500FetchedInstruction fi;
    if (nd500_decode_at(m, CODE_ADDR, &fi) != 0) {
        printf("  ERROR: decode failed - instruction NOT executed\n");
        return 0;
    }
    InstrExecFunc f = g_instr_exec_table[fi.opcode];
    if (!f) {
        printf("  ERROR: no dispatch function for 0x%04X\n", fi.opcode);
        return 0;
    }
    f(cpu, &fi);
    return 1;
}

/* D FCONR $SRC,$DST: FE 84, then two absolute operands (C4 + 32-bit address). */
static uint32_t do_fconr(uint64_t d, uint32_t* st1) {
    Nd500Machine m;
    Nd500Cpu cpu;
    setup(&m, &cpu);
    nd500_bus_write32(&m, SRC_ADDR, (uint32_t)(d >> 32));    /* high word first */
    nd500_bus_write32(&m, SRC_ADDR + 4, (uint32_t)d);
    nd500_bus_write32(&m, DST_ADDR, 0xDEADBEEFu);
    const uint8_t code[] = { 0xFE, 0x84,
                             0xC4, 0x00, 0x00, (uint8_t)(SRC_ADDR >> 8), (uint8_t)SRC_ADDR,
                             0xC4, 0x00, 0x00, (uint8_t)(DST_ADDR >> 8), (uint8_t)DST_ADDR };
    uint32_t r = 0xDEADBEEFu;
    if (run(&m, &cpu, code, sizeof code)) {
        r = nd500_bus_read32(&m, DST_ADDR);
    }
    *st1 = cpu.ST1;
    nd500_machine_free(&m);
    return r;
}

/* F1 AXI $SRC,$INT: FC C0, a single in memory, n a word in memory. */
static uint32_t do_f_axi(uint32_t a, int32_t n, uint32_t* st1) {
    Nd500Machine m;
    Nd500Cpu cpu;
    setup(&m, &cpu);
    nd500_bus_write32(&m, SRC_ADDR, a);
    nd500_bus_write32(&m, INT_ADDR, (uint32_t)n);
    const uint8_t code[] = { 0xFC, 0xC0,
                             0xC4, 0x00, 0x00, (uint8_t)(SRC_ADDR >> 8), (uint8_t)SRC_ADDR,
                             0xC4, 0x00, 0x00, (uint8_t)(INT_ADDR >> 8), (uint8_t)INT_ADDR };
    uint32_t r = 0xDEADBEEFu;
    if (run(&m, &cpu, code, sizeof code)) {
        r = nd500_read_float_register(&cpu, 1);
    }
    *st1 = cpu.ST1;
    nd500_machine_free(&m);
    return r;
}

#define D(sign, exp, mant) (((uint64_t)(sign) << 63) | ((uint64_t)(exp) << 54) | (uint64_t)(mant))
#define F(sign, exp, mant) (((uint32_t)(sign) << 31) | ((uint32_t)(exp) << 22) | (uint32_t)(mant))

static void test_fconr_rule(void) {
    printf("D FCONR: the rounding rule of 7.2.7\n");
    /* Each row: the double, what it shows. The expected single is the
     * reference's; a few are also written out by hand. */
    static const struct { uint64_t d; const char* name; } rows[] = {
        { D(0, 257, 0),                              "1.0 exact" },
        { D(0, 257, 0x0000000080000000ULL),          "G=1 St=0 L=0: half, stays (tie to even)" },
        { D(0, 257, 0x0000000180000000ULL),          "G=1 St=0 L=1: half, up (tie to even)" },
        { D(1, 257, 0x0000000180000000ULL),          "negative, G=1 St=0 L=1: up in magnitude" },
        { D(0, 257, 0x0000000080000001ULL),          "G=1 St=1 L=0: up" },
        { D(0, 257, 0x000000007FFFFFFFULL),          "G=0 St=1: cut" },
        { D(0, 257, 0x0000000100000000ULL),          "only L set: exact" },
        { D(0, 255, 0x15555555555555ULL),            "1/3 as the double 7.2.7 gives" },
        { D(0, 247, 0x0189374BC6A7EFULL),            "about 1.0E-3" },
        { D(0, 300, 0x3FFFFF80000000ULL),            "all 22 ones and G=1: carries into the exponent" },
        { D(1, 300, 0x3FFFFF80000000ULL),            "negative carry into the exponent" },
        { D(0, 1,   0x00000040000000ULL),            "smallest exponent" },
    };
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        uint32_t st1;
        uint32_t got = do_fconr(rows[i].d, &st1);
        check_u64(got, ref_d_to_f(rows[i].d), rows[i].name);
        check_true((st1 & ND500_FLAG_FO) == 0, "no FO");
    }
    uint32_t st1;
    /* By hand, so the reference itself is checked too. */
    check_u64(do_fconr(D(0, 257, 0x0000000080000000ULL), &st1), F(0, 257, 0), "tie, L=0, by hand");
    check_u64(do_fconr(D(0, 257, 0x0000000180000000ULL), &st1), F(0, 257, 2), "tie, L=1, by hand");
    check_u64(do_fconr(D(0, 255, 0x15555555555555ULL), &st1), 0x3FD55555u, "1/3 by hand (G=0: cut)");
    check_u64(do_fconr(D(0, 300, 0x3FFFFF80000000ULL), &st1), F(0, 301, 0), "carry by hand");
    /* Exponent field 0 is exactly zero, whatever the sign and mantissa (7.2.6). */
    check_u64(do_fconr(D(1, 0, 0x123456789ABCDULL), &st1), 0, "exponent 0 gives 0");
    check_true((st1 & ND500_FLAG_Z) != 0, "exponent 0 sets Z");
}

static void test_fconr_overflow(void) {
    printf("D FCONR: rounding up out of the largest single\n");
    uint32_t st1;
    /* Exponent 511, all mantissa ones and G=1: the round carries out of the
     * format, so the result is the largest value with the sign and FO (6.5.1). */
    uint32_t got = do_fconr(D(0, 511, 0x3FFFFF80000000ULL), &st1);
    check_u64(got, 0x7FFFFFFFu, "largest positive single");
    check_true((st1 & ND500_FLAG_FO) != 0, "FO set");
    got = do_fconr(D(1, 511, 0x3FFFFF80000000ULL), &st1);
    check_u64(got, 0xFFFFFFFFu, "largest negative single");
    check_true((st1 & ND500_FLAG_FO) != 0, "FO set (negative)");
    /* The largest double whose G is 0 stays in range. */
    got = do_fconr(D(0, 511, 0x3FFFFF7FFFFFFFULL), &st1);
    check_u64(got, 0x7FFFFFFFu, "largest single, cut");
    check_true((st1 & ND500_FLAG_FO) == 0, "no FO when cut");
}

/* F AXI with n = 2: the microcode squares in double (exact: 23 + 23 bits fit
 * in 55) and then converts with rounding, so the result is a*a rounded once
 * by 7.2.7. The reference forms the exact square of the 23-bit significands
 * and applies the rule at 23 bits. */
static uint32_t ref_square(uint32_t a) {
    uint32_t exponent = (a >> 22) & 0x1FFu;
    if (exponent == 0) {
        return 0;
    }
    uint64_t s = (1ULL << 22) | (a & 0x3FFFFFu);     /* 23 bits, value s * 2**(e-256-23) */
    uint64_t p = s * s;                              /* 45 or 46 bits */
    int e = 2 * ((int)exponent - 256);               /* value = p * 2**(e - 46) */
    int bits = (p >> 45) ? 46 : 45;
    if (bits == 45) {
        e -= 1;                                      /* 0.5 <= M < 1 again */
    }
    int drop = bits - 23;
    uint64_t kept = p >> drop;
    unsigned l = (unsigned)(kept & 1u);
    unsigned g = (unsigned)((p >> (drop - 1)) & 1u);
    unsigned st = (p & ((1ULL << (drop - 1)) - 1)) != 0;
    if (g && (st || l)) {
        kept++;
        if (kept >> 23) {
            kept >>= 1;
            e++;
        }
    }
    return ((uint32_t)(e + 256) << 22) | (uint32_t)(kept & 0x3FFFFFu);
}

static void test_f_axi(void) {
    printf("F AXI: the end of the loop is the double-to-single convert with rounding\n");
    static const uint32_t as[] = {
        F(0, 257, 0x200000),   /* 1.5 */
        F(0, 257, 0x000001),   /* 1 + 2**-22 */
        F(0, 257, 0x3FFFFF),   /* just under 2 */
        F(0, 257, 0x1A2B3C),
        F(0, 250, 0x2F0F0F),
        F(0, 257, 0x0B5000),   /* s = 2**11 * odd: a*a is a tie */
        F(0, 257, 0x16A800),
        F(0, 262, 0x3A5A5A),
    };
    for (size_t i = 0; i < sizeof as / sizeof as[0]; i++) {
        uint32_t st1;
        char what[64];
        snprintf(what, sizeof what, "a=0x%08X ** 2", as[i]);
        check_u64(do_f_axi(as[i], 2, &st1), ref_square(as[i]), what);
    }
    uint32_t st1;
    check_u64(do_f_axi(F(1, 257, 0x200000), 2, &st1), ref_square(F(0, 257, 0x200000)),
              "(-1.5) ** 2 is positive");
    check_u64(do_f_axi(F(0, 257, 0x200000), 1, &st1), F(0, 257, 0x200000), "a ** 1 = a");
    check_u64(do_f_axi(F(0, 257, 0x200000), 0, &st1), F(0, 257, 0), "a ** 0 = 1.0");
    check_u64(do_f_axi(F(0, 258, 0), -1, &st1), F(0, 256, 0), "2.0 ** -1 = 0.5");
    /* 3.0 ** -1: the double divide gives 1/3 rounded at 55 bits; its top
     * 23 bits are 0.10101...01 and G is 0, so the single is cut. */
    check_u64(do_f_axi(F(0, 258, 0x200000), -1, &st1), 0x3FD55555u, "3.0 ** -1");
    check_u64(do_f_axi(0, 3, &st1), 0, "0 ** 3 = 0");
    /* A large power overflows: the largest value with the sign, FO. */
    check_u64(do_f_axi(F(1, 400, 0), 3, &st1), 0xFFFFFFFFu, "(-big) ** 3 overflows negative");
    check_true((st1 & ND500_FLAG_FO) != 0, "FO set");
}

int main(void) {
    test_fconr_rule();
    test_fconr_overflow();
    test_f_axi();
    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
