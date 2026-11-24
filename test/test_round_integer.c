/*
 * Rounding a float to a whole number: INTR (ND-05.009.4 10.35) and WCONR,
 * HCONR, BYCONR (15.3) say "The result is rounded". Owner decision
 * 2026-11-24: the 7.2.7 rule applied at the integer position, "if G=1 and
 * (St=1 or L=1) then add one" with L the units bit, on the magnitude. So 0.5
 * gives 0, 1.5 gives 2, 2.5 gives 2, -1.5 gives -2. RetroCore has the same
 * cases in TestND500_TrueValueRounding.
 *
 * F1 INTR R2 (0FE68H, operand D1): A2 = source, A1 = result.
 * F WCONR R1, R2 (0FE74H D0 D1), F HCONR (0FE72H), F BYCONR (0FE70H):
 * A1 = source, I2 = destination.
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

static int tests_passed = 0;
static int tests_failed = 0;

/* Run one instruction; returns A1 for INTR, I2 for the CONR forms. */
static uint32_t run(const uint8_t *code, unsigned n, uint32_t a1, uint32_t a2)
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
    cpu.A[1] = a2;
    cpu.I[1] = 0u;
    cpu.ST1 = 0u;
    m.run_flag = 1;
    nd500_cpu_step(&cpu);
    uint32_t r = code[1] == 0x68 ? cpu.A[0] : cpu.I[1];
    nd500_machine_free(&m);
    return r;
}

/* D1 INTR R2 (0FE6CH, operand D1 = register 2): D2 = A2:E2 the source
 * (A the high half), D1 = A1:E1 the result. */
static uint64_t run_d(uint64_t x)
{
    static const uint8_t code[] = {0xFE, 0x6C, 0xD1};
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    for (unsigned i = 0u; i < sizeof code; i++) {
        nd500_bus_write8(&m, CODE_ADDR + i, code[i]);
    }
    cpu.PC = CODE_ADDR;
    cpu.A[0] = 0u;
    cpu.E[0] = 0u;
    cpu.A[1] = (uint32_t)(x >> 32);
    cpu.E[1] = (uint32_t)x;
    cpu.ST1 = 0u;
    m.run_flag = 1;
    nd500_cpu_step(&cpu);
    uint64_t r = ((uint64_t)cpu.A[0] << 32) | cpu.E[0];
    nd500_machine_free(&m);
    return r;
}

/* D1 INTR with the operand in memory by the ABSOLUTE address code (0C4H and a
 * 4-byte address): both words of the double are read, high word first. */
static uint64_t run_d_mem(uint64_t x)
{
    static const uint8_t code[] = {0xFE, 0x6C, 0xC4, 0x00, 0x00, 0x20, 0x00};
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    for (unsigned i = 0u; i < sizeof code; i++) {
        nd500_bus_write8(&m, CODE_ADDR + i, code[i]);
    }
    for (unsigned i = 0u; i < 8u; i++) {
        nd500_bus_write8(&m, 0x2000u + i, (uint8_t)(x >> (56u - 8u * i)));
    }
    cpu.PC = CODE_ADDR;
    cpu.A[0] = 0u;
    cpu.E[0] = 0u;
    cpu.ST1 = 0u;
    m.run_flag = 1;
    nd500_cpu_step(&cpu);
    uint64_t r = ((uint64_t)cpu.A[0] << 32) | cpu.E[0];
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
    static const uint8_t intr[] = {0xFE, 0x68, 0xD1};
    static const uint8_t wconr[] = {0xFE, 0x74, 0xD0, 0xD1};
    static const uint8_t hconr[] = {0xFE, 0x72, 0xD0, 0xD1};
    static const uint8_t byconr[] = {0xFE, 0x70, 0xD0, 0xD1};
    printf("ND500 rounding to a whole number (10.35, 15.3, 7.2.7)\n");
    check("F INTR 0.5 -> 0", run(intr, 3, 0, 0x40000000u), 0x00000000u);
    check("F INTR 1.5 -> 2", run(intr, 3, 0, 0x40600000u), 0x40800000u);
    check("F INTR 2.5 -> 2", run(intr, 3, 0, 0x40900000u), 0x40800000u);
    check("F INTR 3.5 -> 4", run(intr, 3, 0, 0x40B00000u), 0x40C00000u);
    check("F INTR 0.75 -> 1", run(intr, 3, 0, 0x40200000u), 0x40400000u);
    check("F INTR -0.5 -> 0", run(intr, 3, 0, 0xC0000000u), 0x00000000u);
    check("F INTR -2.5 -> -2", run(intr, 3, 0, 0xC0900000u), 0xC0800000u);
    check64("D INTR 0.5 -> 0", run_d(0x4000000000000000ull), 0ull);
    check64("D INTR 2.5 -> 2", run_d(0x4090000000000000ull), 0x4080000000000000ull);
    check64("D INTR -1.5 -> -2", run_d(0xC060000000000000ull), 0xC080000000000000ull);
    check64("D INTR just above 2.5 -> 3", run_d(0x4090000000000001ull), 0x40A0000000000000ull);
    check64("D INTR memory just above 2.5 -> 3", run_d_mem(0x4090000000000001ull), 0x40A0000000000000ull);
    check64("D INTR memory 2.5 -> 2", run_d_mem(0x4090000000000000ull), 0x4080000000000000ull);
    check("F WCONR 0.5 -> 0", run(wconr, 4, 0x40000000u, 0), 0u);
    check("F WCONR 1.5 -> 2", run(wconr, 4, 0x40600000u, 0), 2u);
    check("F WCONR 2.5 -> 2", run(wconr, 4, 0x40900000u, 0), 2u);
    check("F WCONR 1.75 -> 2", run(wconr, 4, 0x40700000u, 0), 2u);
    check("F WCONR -1.5 -> -2", run(wconr, 4, 0xC0600000u, 0), 0xFFFFFFFEu);
    check("F HCONR 2.5 -> 2", run(hconr, 4, 0x40900000u, 0) & 0xFFFFu, 2u);
    check("F BYCONR -3.5 -> -4", run(byconr, 4, 0xC0B00000u, 0) & 0xFFu, 0xFCu);
    printf("Passed: %d\nFailed: %d\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
