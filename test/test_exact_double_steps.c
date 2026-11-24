/*
 * D POLY (ND-05.009.4 12.3), D REM (10.33), D INCR (10.19) and D ENTIER
 * (10.37) on operands whose lowest mantissa bits decide the result. A D value
 * has 55 significant bits (7.2.6: sign, 9-bit exponent field, 54 stored
 * mantissa bits after the leading 1); a host double has 53. Until 2026-11-24
 * all four went through a host double, so each case below came out wrong.
 * RetroCore has the same cases in TestND500_ExactDoubleSteps.
 *
 * D format, worked by hand from 7.2.6: value = 2**(e - 256) * 0.1mmm..., e the
 * exponent field. 1.0 = 4040000000000000H (e = 257), its last mantissa bit is
 * worth 2**-54; 2**-54 alone is e = 203: 32C0000000000000H. The sign is bit
 * 63 (sign and magnitude, 7.2.5).
 *
 * Operand bytes (Appendix A): 0D0H + y is register y + 1 of the operand's
 * type (D register n = An:En, A the high word); 00H..3FH a short constant.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/nd500_instructions.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/instruction_helpers.h"

#define MEMORY_SIZE (1 << 20)
#define CODE_ADDR 0x1000u
#define ST_S 0x80u
#define ST_DATA 0x3E0u   /* Z C S K O: bits 5 .. 9 */

static int tests_passed = 0;
static int tests_failed = 0;

typedef struct {
    uint64_t d[4];      /* D1 .. D4 after the instruction */
    uint32_t i2;        /* W2 after the instruction */
    uint32_t st1;
} Result;

/* Run one instruction with D1 .. D4 set from d_in (A high, E low). */
static Result run(const uint8_t *code, unsigned n, const uint64_t d_in[4])
{
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    for (unsigned i = 0u; i < n; i++) {
        nd500_bus_write8(&m, CODE_ADDR + i, code[i]);
    }
    cpu.PC = CODE_ADDR;
    for (int k = 0; k < 4; k++) {
        cpu.A[k] = (uint32_t)(d_in[k] >> 32);
        cpu.E[k] = (uint32_t)d_in[k];
    }
    cpu.I[1] = 0u;
    cpu.ST1 = 0u;
    m.run_flag = 1;
    nd500_cpu_step(&cpu);
    Result r;
    for (int k = 0; k < 4; k++) {
        r.d[k] = ((uint64_t)cpu.A[k] << 32) | cpu.E[k];
    }
    r.i2 = cpu.I[1];
    r.st1 = cpu.ST1;
    nd500_machine_free(&m);
    return r;
}

static void check64(const char *name, uint64_t got, uint64_t want)
{
    if (got == want) {
        printf("  [PASS] %s\n", name);
        tests_passed++;
    } else {
        printf("  [FAIL] %s: got 0x%016llX want 0x%016llX\n", name,
               (unsigned long long)got, (unsigned long long)want);
        tests_failed++;
    }
}

static void check(const char *name, uint32_t got, uint32_t want)
{
    if (got == want) {
        printf("  [PASS] %s\n", name);
        tests_passed++;
    } else {
        printf("  [FAIL] %s: got 0x%08X want 0x%08X\n", name, got, want);
        tests_failed++;
    }
}

int main(void)
{
    const uint64_t one_plus_ulp = 0x4040000000000001ull;   /* 1 + 2**-54 */
    const uint64_t one = 0x4040000000000000ull;
    const uint64_t minus_one = 0xC040000000000000ull;
    const uint64_t two_pow_m54 = 0x32C0000000000000ull;
    printf("ND500 D POLY, REM, INCR, ENTIER on the 54th and 55th mantissa bits\n");

    /* D1 POLY 0FCE4H: x = D2, <m> = short constant 1, c1 = D3, c0 = D4.
     * 1.0 * (1 + 2**-54) - 1.0 = 2**-54 exactly. */
    {
        static const uint8_t code[] = {0xFC, 0xE4, 0xD1, 0x01, 0xD2, 0xD3};
        const uint64_t in[4] = {0, one_plus_ulp, one, minus_one};
        Result r = run(code, sizeof code, in);
        check64("D POLY 1*(1+2**-54) - 1 = 2**-54", r.d[0], two_pow_m54);
        check("D POLY data status clear", r.st1 & ST_DATA, 0u);
    }
    /* D1 REM 0FE5CH: x = D2, y = D3, q = D4. x/y = x, int = 1.0 -> q,
     * x - q*y = 2**-54 -> D1. */
    {
        static const uint8_t code[] = {0xFE, 0x5C, 0xD1, 0xD2, 0xD3};
        const uint64_t in[4] = {0, one_plus_ulp, one, 0};
        Result r = run(code, sizeof code, in);
        check64("D REM (1+2**-54) / 1: remainder 2**-54", r.d[0], two_pow_m54);
        check64("D REM (1+2**-54) / 1: q = 1.0", r.d[3], one);
        check("D REM data status clear", r.st1 & ST_DATA, 0u);
    }
    /* D INCR 0FC8BH, operand D1: -(0.5 + 2**-55) + 1 = 0.5 - 2**-55 =
     * 3FFFFFFFFFFFFFFEH (e = 255, mantissa all ones but the last). */
    {
        static const uint8_t code[] = {0xFC, 0x8B, 0xD0};
        const uint64_t in[4] = {0xC000000000000001ull, 0, 0, 0};
        Result r = run(code, sizeof code, in);
        check64("D INCR -(0.5+2**-55) = 0.5-2**-55", r.d[0], 0x3FFFFFFFFFFFFFFEull);
        check("D INCR data status clear", r.st1 & ST_DATA, 0u);
    }
    /* D ENTIER 0FDC8H: source D1, destination W2. */
    {
        static const uint8_t code[] = {0xFD, 0xC8, 0xD0, 0xD1};
        const uint64_t below_four[4] = {0x40BFFFFFFFFFFFFFull, 0, 0, 0};   /* 4 - 2**-53 */
        Result r = run(code, sizeof code, below_four);
        check("D ENTIER 4 - 2**-53 = 3", r.i2, 3u);
        check("D ENTIER 4 - 2**-53 data status clear", r.st1 & ST_DATA, 0u);
        const uint64_t below_m1[4] = {0xC040000000000001ull, 0, 0, 0};    /* -(1 + 2**-54) */
        r = run(code, sizeof code, below_m1);
        check("D ENTIER -(1 + 2**-54) = -2", r.i2, 0xFFFFFFFEu);
        check("D ENTIER -(1 + 2**-54) S", r.st1 & ST_DATA, ST_S);
    }
    printf("Passed: %d\nFailed: %d\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
