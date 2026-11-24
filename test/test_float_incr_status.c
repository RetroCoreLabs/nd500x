/*
 * F and D INCR (ND-05.009.4 10.19) status bits:
 *  - 10.19 trap conditions: "Integer overflow (O)" only; no floating
 *    overflow is named.
 *  - 6.5.1: "Integer Overflow may be set only when performing integer
 *    arithmetic; otherwise it is cleared", and "All data status bits not
 *    mentioned are reset".
 *  - Appendix I (page 403, checked on the scan): the INCR row reads O = I
 *    ("set or reset if integer instruction, otherwise cleared"), IVO, DZ, FU,
 *    FO, BO = C ("unconditionally cleared").
 *  - 7.2.7: a sum is rounded up only when the guard bit G is 1. The largest F
 *    or D value plus 1 has G = 0, so it rounds back to the largest value and
 *    no float INCR can overflow.
 * So a float INCR leaves C, O, FU and FO clear whatever they held before.
 * RetroCore has the same cases in TestND500_FloatIncrStatus.
 *
 * Operand byte 0D0H is register 1 of the operand's type (D1 = A1:E1).
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
/* C (6), O (9), FU (13), FO (14) set before every case */
#define ST_BEFORE ((1u << 6) | (1u << 9) | (1u << 13) | (1u << 14))
#define ST_DATA 0xFFE0u   /* data status bits 5 .. 15 (6.5.1) */

static int tests_passed = 0;
static int tests_failed = 0;

static uint32_t run(const uint8_t *code, unsigned n, uint32_t a1, uint32_t e1, uint64_t *d1)
{
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    for (unsigned i = 0u; i < n; i++) {
        nd500_bus_write8(&m, CODE_ADDR + i, code[i]);
    }
    cpu.PC = CODE_ADDR;
    cpu.A[0] = a1;
    cpu.E[0] = e1;
    cpu.ST1 = ST_BEFORE;
    m.run_flag = 1;
    nd500_cpu_step(&cpu);
    *d1 = ((uint64_t)cpu.A[0] << 32) | cpu.E[0];
    uint32_t st = cpu.ST1;
    nd500_machine_free(&m);
    return st;
}

static void check(const char *name, uint64_t got, uint64_t want)
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

int main(void)
{
    static const uint8_t f_incr[] = {0x50, 0xD0};         /* F INCR F1 */
    static const uint8_t d_incr[] = {0xFC, 0x8B, 0xD0};   /* D INCR D1 */
    uint64_t d1;
    uint32_t st;
    printf("ND500 F and D INCR status bits\n");

    st = run(f_incr, sizeof f_incr, 0x40400000u, 0u, &d1);   /* 1.0 + 1 = 2.0 */
    check("F INCR 1.0 = 2.0", d1 >> 32, 0x40800000u);
    check("F INCR 1.0: data status clear", st & ST_DATA, 0u);

    st = run(f_incr, sizeof f_incr, 0x7FFFFFFFu, 0u, &d1);   /* largest F */
    check("F INCR largest stays largest", d1 >> 32, 0x7FFFFFFFu);
    check("F INCR largest: no O, no FO", st & ST_DATA, 0u);

    st = run(d_incr, sizeof d_incr, 0x7FFFFFFFu, 0xFFFFFFFFu, &d1);   /* largest D */
    check("D INCR largest stays largest", d1, 0x7FFFFFFFFFFFFFFFull);
    check("D INCR largest: no O, no FO", st & ST_DATA, 0u);

    printf("Passed: %d\nFailed: %d\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
