/*
 * ND-05.009.4 15.2 (data type conversion), data status bits: "result = 0 -> Z",
 * "result.signbit -> S". For HCONV the result is a halfword, so S is bit 15 of
 * the result for every source type, W HCONV included.
 *
 * W HCONV R1, R2 (0FD55H, operands D0 D1): I1 = source, I2 = destination.
 * The same values are in RetroCore's corpus file FLOAT_MATH_hconv.json.
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

static void run_case(const char *name, uint32_t source, uint32_t want_i2, bool want_z, bool want_s)
{
    static const uint8_t code[] = {0xFD, 0x55, 0xD0, 0xD1}; /* W HCONV R1, R2 */
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    for (uint32_t i = 0u; i < sizeof code; i++) {
        nd500_bus_write8(&m, CODE_ADDR + i, code[i]);
    }
    cpu.PC = CODE_ADDR;
    cpu.I[0] = source;
    cpu.I[1] = 0u;
    cpu.ST1 = 0u;
    m.run_flag = 1;

    nd500_cpu_step(&cpu);

    bool z = nd500_test_flag(&cpu, ND500_FLAG_Z);
    bool s = nd500_test_flag(&cpu, ND500_FLAG_S);
    if ((cpu.I[1] & 0xFFFFu) == want_i2 && z == want_z && s == want_s) {
        printf("  [PASS] %s\n", name);
        tests_passed++;
    } else {
        printf("  [FAIL] %s: I2=0x%08X Z=%d (want %d) S=%d (want %d)\n",
               name, cpu.I[1], z, want_z, s, want_s);
        tests_failed++;
    }
    nd500_machine_free(&m);
}

int main(void)
{
    printf("ND500 W HCONV status bits (ND-05.009.4 15.2)\n");
    run_case("0x0000ABCD -> 0xABCD, S=1", 0x0000ABCDu, 0xABCDu, false, true);
    run_case("0xFFFF8000 -> 0x8000, S=1", 0xFFFF8000u, 0x8000u, false, true);
    run_case("0xFFFFFFFF -> 0xFFFF, S=1", 0xFFFFFFFFu, 0xFFFFu, false, true);
    run_case("0x00001234 -> 0x1234, S=0", 0x00001234u, 0x1234u, false, false);
    run_case("0x00010000 -> 0x0000, Z=1", 0x00010000u, 0x0000u, true, false);
    printf("Passed: %d\nFailed: %d\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
