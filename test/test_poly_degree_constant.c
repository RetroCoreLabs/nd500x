/*
 * POLY degree operand (ND-05.009.4 12.3): "<m> must always be a positive
 * constant less than 256, otherwise an illegal operand specifier trap
 * condition occurs." Trap conditions: "... Illegal operand specifier (IOS)".
 *
 * Each case is "F1 POLY B.8, <m>, B.16" (opcode 0FCE0H) with <m> one of:
 *  - short constant $1 (byte 01H): no trap, two coefficients follow;
 *  - absolute address 0 (address code 0C4H, 4-byte address 00000000H);
 *  - B-relative B.24 (local short, byte 46H);
 *  - register R1 (byte 0D0H).
 * The three non-constant forms must raise IOS and nothing else. Before this
 * check existed the decoder took the absolute and register forms as degree 0
 * and the instruction ran whenever the byte it read as the degree was 0.
 * RetroCore has the same cases in TestND500_PolyDegreeConstant.
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
#define B_ADDR 0x2000u

static int tests_passed = 0;
static int tests_failed = 0;

/* Runs one instruction; returns the trap condition bits raised (0 if none). */
static uint64_t run(const uint8_t *code, unsigned n)
{
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    for (unsigned i = 0u; i < n; i++) {
        nd500_bus_write8(&m, CODE_ADDR + i, code[i]);
    }
    /* x = 3.0 at B+8; coefficients 2.0, 1.0 at B+16, B+24; all memory else 0 */
    static const uint8_t mem[] = {0x40, 0xA0, 0, 0, 0, 0, 0, 0,
                                  0x40, 0x80, 0, 0, 0, 0, 0, 0,
                                  0x40, 0x40, 0, 0};
    for (unsigned i = 0u; i < sizeof mem; i++) {
        nd500_bus_write8(&m, B_ADDR + 8u + i, mem[i]);
    }
    cpu.PC = CODE_ADDR;
    cpu.B = B_ADDR;
    m.run_flag = 1;
    nd500_trap_clear();
    nd500_cpu_step(&cpu);
    uint64_t bits = 0;
    if (nd500_trap_occurred()) {
        bits = nd500_trap_get_state()->trap_condition;
        nd500_trap_clear();
    }
    nd500_machine_free(&m);
    return bits;
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
    /* F1 POLY B.8, $1, B.16, B.24: 2x + 1 */
    static const uint8_t m_const[] = {0xFC, 0xE0, 0x42, 0x01, 0x44, 0x46};
    /* F1 POLY B.8, 0 (absolute), B.16 */
    static const uint8_t m_abs[] = {0xFC, 0xE0, 0x42, 0xC4, 0x00, 0x00, 0x00, 0x00, 0x44};
    /* F1 POLY B.8, B.24, B.16 */
    static const uint8_t m_local[] = {0xFC, 0xE0, 0x42, 0x46, 0x44};
    /* F1 POLY B.8, R1, B.16 */
    static const uint8_t m_reg[] = {0xFC, 0xE0, 0x42, 0xD0, 0x44};
    printf("ND500 POLY degree must be a constant\n");

    check("POLY degree $1: no trap", run(m_const, sizeof m_const), 0u);
    check("POLY degree absolute 0: IOS", run(m_abs, sizeof m_abs), TRAP_IOS);
    check("POLY degree B.24: IOS", run(m_local, sizeof m_local), TRAP_IOS);
    check("POLY degree R1: IOS", run(m_reg, sizeof m_reg), TRAP_IOS);

    printf("Passed: %d\nFailed: %d\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
