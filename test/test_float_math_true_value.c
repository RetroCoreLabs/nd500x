/*
 * The mathematical functions (ND-05.009.4 chapter 12): each result is the
 * true value of the function rounded once by the 7.2.7 rule ("if G=1 and
 * (St=1 or L=1) then add one"), for F and D. Owner decision 2026-11-24.
 * The manual names each function and gives no algorithm and no rounding
 * rule of its own.
 *
 * Expected values: RetroCore tools/nd500_true_value.py (Python decimal at
 * 200 digits, the "near" column), the same values as RetroCore
 * TestND500_TrueValueRounding.F_Math_TrueValue / D_Math_TrueValue, and the
 * corpus cases recorded again on 2026-11-24 (Comprehensive alog2, cos, exp,
 * tan; FloatMath atan 100 / -100). The invalid-operand rows are the values
 * the sections give: 12.6/12.8 IVO and 0, 12.12 IVO and the largest value,
 * 12.13 IVO and -5.8*10**76 (all bits set).
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../src/cpu/float_math.h"
#include "../src/cpu/float_exact.h"

static int tests_passed = 0;
static int tests_failed = 0;

enum { SIN, COS, TAN, ASIN, ACOS, ATAN, EXP, ALOG, ALOG2, ALOG10 };

static uint64_t call(int fn, uint64_t x, bool d, unsigned* exc)
{
    switch (fn) {
    case SIN: return nd500_fm_sin(x, d, exc);
    case COS: return nd500_fm_cos(x, d, exc);
    case TAN: return nd500_fm_tan(x, d, exc);
    case ASIN: return nd500_fm_asin(x, d, exc);
    case ACOS: return nd500_fm_acos(x, d, exc);
    case ATAN: return nd500_fm_atan2(x, nd500_fm_one(d), d, exc);
    case EXP: return nd500_fm_exp(x, d, exc);
    case ALOG: return nd500_fm_alog(x, 0, d, exc);
    case ALOG2: return nd500_fm_alog(x, 2, d, exc);
    default: return nd500_fm_alog(x, 10, d, exc);
    }
}

typedef struct {
    const char* name;
    int fn;
    bool d;
    uint64_t x;
    uint64_t want;
    bool ivo;
} Case;

static const Case cases[] = {
    /* F, true value rounded by 7.2.7 */
    { "F ACOS 0.5", ACOS, false, 0x40000000u, 0x40430549u, false },
    { "F ATAN 0.5", ATAN, false, 0x40000000u, 0x3FF6B19Cu, false },
    { "F ASIN 0.707", ASIN, false, 0x401A7EFAu, 0x402482FBu, false },
    { "F EXP -1", EXP, false, 0xC0400000u, 0x3FDE2D59u, false },
    { "F COS 1", COS, false, 0x40400000u, 0x400528A0u, false },
    { "F TAN 0.5236", TAN, false, 0x40030549u, 0x4009E69Du, false },
    { "F ALOG10 1000", ALOG10, false, 0x42BD0000u, 0x40A00000u, false },
    { "F ATAN 100", ATAN, false, 0x41E40000u, 0x4063E418u, false },
    { "F ATAN -100", ATAN, false, 0xC1E40000u, 0xC063E418u, false },
    { "F ATAN 72", ATAN, false, 0x41C80000u, 0x4063A463u, false },
    { "F SIN 2", SIN, false, 0x40800000u, 0x403463DCu, false },
    { "F ALOG 2", ALOG, false, 0x40800000u, 0x4018B90Cu, false },
    { "F ALOG2 1.5", ALOG2, false, 0x40600000u, 0x400AE00Du, false },
    { "F ALOG2 3.7e-56", ALOG2, false, 0x12345678u, 0xC21C11A4u, false },
    { "F COS 0.125", COS, false, 0x3F800000u, 0x403F0055u, false },
    { "F COS -0.125", COS, false, 0xBF800000u, 0x403F0055u, false },
    { "F EXP -0.125", EXP, false, 0xBF800000u, 0x4030F5A9u, false },
    { "F TAN 0.5", TAN, false, 0x40000000u, 0x4005ED3Du, false },
    { "F SIN 1.6889", SIN, false, 0x406C16CCu, 0x403F1BC4u, false },
    { "F SIN 1.9733", SIN, false, 0x407E4B23u, 0x4035C4D2u, false },
    { "F ALOG 0.7756", ALOG, false, 0x40234570u, 0xBFC11192u, false },
    { "F ALOG 1.4044", ALOG, false, 0x4059E275u, 0x3FD6F2E1u, false },
    /* D */
    { "D ACOS 0.5", ACOS, true, 0x4000000000000000ull, 0x40430548E0B5CD96ull, false },
    { "D ATAN 0.5", ATAN, true, 0x4000000000000000ull, 0x3FF6B19C1586ED3Eull, false },
    { "D ATAN 1 (pi/4)", ATAN, true, 0x4040000000000000ull, 0x402487ED5110B461ull, false },
    { "D ATAN 3.9e25 (pi/2)", ATAN, true, 0x5555555500000000ull, 0x406487ED5110B461ull, false },
    { "D ASIN 0.707", ASIN, true, 0x401A7EFA00000000ull, 0x402482FAD727B980ull, false },
    { "D EXP -1", EXP, true, 0xC040000000000000ull, 0x3FDE2D58D8B3BCDFull, false },
    { "D COS 1", COS, true, 0x4040000000000000ull, 0x400528A03ED41A2Eull, false },
    { "D TAN 0.5236", TAN, true, 0x4003054900000000ull, 0x4009E69D3FF90FAFull, false },
    { "D ALOG10 1000", ALOG10, true, 0x42BD000000000000ull, 0x40A0000000000000ull, false },
    { "D ATAN 72", ATAN, true, 0x41C8000000000000ull, 0x4063A462D6FB8DB8ull, false },
    { "D SIN 2", SIN, true, 0x4080000000000000ull, 0x403463DBAB46D117ull, false },
    { "D ALOG 2", ALOG, true, 0x4080000000000000ull, 0x4018B90BFBE8E7BDull, false },
    { "D ALOG2 1.5", ALOG2, true, 0x4060000000000000ull, 0x400AE00D1CFDEB44ull, false },
    { "D ACOS 0 (pi/2)", ACOS, true, 0, 0x406487ED5110B461ull, false },
    /* exactly-zero values and invalid operands */
    { "F ASIN 0", ASIN, false, 0, 0, false },
    { "F ACOS 1", ACOS, false, 0x40400000u, 0, false },
    { "D ALOG 1", ALOG, true, 0x4040000000000000ull, 0, false },
    { "F ASIN 1.5 (IVO)", ASIN, false, 0x40600000u, 0, true },
    { "F ALOG 0 (IVO)", ALOG, false, 0, 0xFFFFFFFFu, true },
    { "F ALOG -2 (IVO)", ALOG, false, 0xC0800000u, 0xFFFFFFFFu, true },
    { "F EXP 200 (IVO)", EXP, false, 0x42190000u, 0x7FFFFFFFu, true },
    { "F EXP -200 (0)", EXP, false, 0xC2190000u, 0, false },
    { "F SIN 1e6 (IVO)", SIN, false, 0x4A7A1200u, 0, true },
};

int main(void)
{
    printf("ND500 math functions: true value rounded once by 7.2.7 (chapter 12)\n");
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const Case* c = &cases[i];
        unsigned exc = 0;
        uint64_t got = call(c->fn, c->x, c->d, &exc);
        bool ivo = (exc & ND500_FX_IVO) != 0u;
        if (got == c->want && ivo == c->ivo) {
            printf("  [PASS] %s\n", c->name);
            tests_passed++;
        } else {
            printf("  [FAIL] %s: got 0x%016llX%s want 0x%016llX%s\n", c->name,
                   (unsigned long long)got, ivo ? " IVO" : "",
                   (unsigned long long)c->want, c->ivo ? " IVO" : "");
            tests_failed++;
        }
    }
    printf("Passed: %d\nFailed: %d\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
