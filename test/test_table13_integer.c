/**
 * ND-05.009.4 Table 13, integer rows: a constant wider than the instruction's
 * operand type is an illegal operand specifier (IOS) trap condition.
 *
 *   BI: :B :H :W :F :D are IOS     BY: :W :F :D are IOS
 *   H:  :W :F :D are IOS           W:  :D is IOS
 *
 * BY with a :H constant is NOT IOS here, against Table 13 (ND-05.009.4 page
 * 119): owner decision 2026-11-24, because the real PLANC-500 G compiler writes
 * BY := 128 as 06 CE 00 80. The byte operand takes the low byte of the halfword.
 *
 * Constant codes (ND-05.009.4 8.12): :B 0CDH, :H 0CEH, :W/:F 0CFH, :D 0CCH.
 * The bytes are the RetroCore corpus case ARITHMETIC__add_.json
 * "BY1 + $305419896" and its relatives.
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

/* Run one instruction with I1 = 0x7F; return whether IOS was raised and I<reg> after. */
static bool run(const uint8_t *code, uint32_t len, uint32_t *i1_after, int reg)
{
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    for (uint32_t i = 0u; i < len; i++) {
        nd500_bus_write8(&m, CODE_ADDR + i, code[i]);
    }
    cpu.PC = CODE_ADDR;
    cpu.I[0] = 0x7Fu;
    cpu.ST1 = 0u;
    cpu.ST2 = 0u;
    m.run_flag = 1;
    nd500_trap_clear();   /* the trap state is global; start each case clean */
    nd500_cpu_step(&cpu);
    *i1_after = cpu.I[reg - 1];
    bool ios = (cpu.ST2 & (uint32_t)(TRAP_IOS >> 32)) != 0u;
    nd500_machine_free(&m);
    return ios;
}

static void expect_reg(const char *name, const uint8_t *code, uint32_t len, bool want_ios, uint32_t want_i1, int reg)
{
    uint32_t i1 = 0u;
    bool ios = run(code, len, &i1, reg);
    if (ios == want_ios && i1 == want_i1) {
        printf("  [PASS] %s\n", name);
        tests_passed++;
    } else {
        printf("  [FAIL] %s: IOS %d (want %d), I1 0x%X (want 0x%X)\n", name, ios, want_ios, i1, want_i1);
        tests_failed++;
    }
}

static void expect(const char *name, const uint8_t *code, uint32_t len, bool want_ios, uint32_t want_i1)
{
    expect_reg(name, code, len, want_ios, want_i1, 1);
}

int main(void)
{
    static const uint8_t by_w[] = {0xFC, 0x34, 0xCF, 0x12, 0x34, 0x56, 0x78};
    static const uint8_t by_h[] = {0xFC, 0x34, 0xCE, 0x12, 0x34};
    static const uint8_t h_w[]  = {0xFC, 0x38, 0xCF, 0x12, 0x34, 0x56, 0x78};
    static const uint8_t by_set_128[] = {0x06, 0xCE, 0x00, 0x80};
    static const uint8_t by_b[] = {0xFC, 0x34, 0xCD, 0x01};

    printf("ND500 Table 13 integer constant rows\n");
    expect("BY1 + :W constant is IOS, I1 unchanged", by_w, sizeof(by_w), true, 0x7Fu);
    /* 0x7F + low byte 0x34 = 0xB3 */
    expect("BY1 + :H constant runs with the low byte (owner decision)", by_h, sizeof(by_h), false, 0xB3u);
    /* The exact bytes the real PLANC-500 G compiler writes for BY := 128. */
    expect_reg("06 CE 00 80 (BY3 := :H 128) gives 128 in I3", by_set_128, sizeof(by_set_128), false, 0x80u, 3);
    expect("H1 + :W constant is IOS, I1 unchanged", h_w, sizeof(h_w), true, 0x7Fu);
    expect("BY1 + :B constant is accepted (NC)", by_b, sizeof(by_b), false, 0x80u);
    printf("Passed: %d\nFailed: %d\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
