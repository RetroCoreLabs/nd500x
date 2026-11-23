/**
 * ND500 F (float) element tests for SMOVE, SMOVN and BMOVE.
 *
 * An F element is 32 bits. ND-05.009.4: F SMOVE 0FD6AH "float string move",
 * F SMOVN 0FD7AH, F BMOVE 0FE7AH "float block move". nd500x read every F
 * string element as 0 (nd500_read_value_at_address had no F case) and F BMOVE
 * stopped with "Invalid data type 4" and moved nothing.
 *
 * The cases are the shape of the RetroCore corpus cases STRING_smove.json
 * "F SMOVE B.100, B.200": B = 0x2000, the descriptors at B.100 and B.200,
 * 4 elements from 0x4000 to 0x5000.
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
#define B_ADDR    0x2000u
#define SRC_ADDR  0x4000u
#define DST_ADDR  0x5000u

static int tests_passed = 0;
static int tests_failed = 0;

static void put_be32(Nd500Machine *m, uint32_t addr, uint32_t v)
{
    nd500_bus_write8(m, addr + 0u, (uint8_t)(v >> 24));
    nd500_bus_write8(m, addr + 1u, (uint8_t)(v >> 16));
    nd500_bus_write8(m, addr + 2u, (uint8_t)(v >> 8));
    nd500_bus_write8(m, addr + 3u, (uint8_t)v);
}

/* 16 source bytes 1..16 (four F elements), 16 destination bytes 0. */
static void setup(Nd500Machine *m, Nd500Cpu *cpu, const uint8_t *code, uint32_t len)
{
    nd500_machine_init(m, MEMORY_SIZE);
    nd500_cpu_init(cpu, m);
    for (uint32_t i = 0u; i < 16u; i++) {
        nd500_bus_write8(m, SRC_ADDR + i, (uint8_t)(i + 1u));
        nd500_bus_write8(m, DST_ADDR + i, 0u);
    }
    /* descriptors (ND-05.009.4 7.2.8): element count, then address */
    put_be32(m, B_ADDR + 100u, 4u);
    put_be32(m, B_ADDR + 104u, SRC_ADDR);
    put_be32(m, B_ADDR + 200u, 4u);
    put_be32(m, B_ADDR + 204u, DST_ADDR);
    for (uint32_t i = 0u; i < len; i++) {
        nd500_bus_write8(m, CODE_ADDR + i, code[i]);
    }
    cpu->PC = CODE_ADDR;
    cpu->B = B_ADDR;
    cpu->I[0] = 0u;
    cpu->I[1] = 0u;
    m->run_flag = 1;
}

static void check_copied(Nd500Machine *m, const char *name, uint32_t dst)
{
    bool ok = true;
    for (uint32_t i = 0u; i < 16u; i++) {
        uint8_t b = nd500_bus_read8(m, dst + i);
        if (b != (uint8_t)(i + 1u)) {
            printf("  [FAIL] %s: byte %u = 0x%02X, expected 0x%02X\n", name, i, b, i + 1u);
            ok = false;
            break;
        }
    }
    if (ok) {
        printf("  [PASS] %s\n", name);
        tests_passed++;
    } else {
        tests_failed++;
    }
}

static void test_f_smove(void)
{
    /* F SMOVE B.100, B.200: 0FD6AH, B.25 words, B.50 words */
    static const uint8_t code[] = {0xFD, 0x6A, 0x59, 0x72};
    Nd500Machine m;
    Nd500Cpu cpu;
    setup(&m, &cpu, code, sizeof(code));
    nd500_cpu_step(&cpu);
    check_copied(&m, "F SMOVE copies four 32-bit elements", DST_ADDR);
    nd500_machine_free(&m);
}

static void test_f_smovn(void)
{
    /* F SMOVN B.100, B.200, 4: 0FD7AH, short constant 4 */
    static const uint8_t code[] = {0xFD, 0x7A, 0x59, 0x72, 0x04};
    Nd500Machine m;
    Nd500Cpu cpu;
    setup(&m, &cpu, code, sizeof(code));
    nd500_cpu_step(&cpu);
    check_copied(&m, "F SMOVN copies four 32-bit elements", DST_ADDR);
    nd500_machine_free(&m);
}

static void test_f_bmove(void)
{
    /* F BMOVE R1.(0), R1.(0x1000), 4 is not encodable in one byte, so use
     * pre-indexed R1.(0) with I1 = SRC and a second form R2.(0) with I2 = DST. */
    static const uint8_t code[] = {0xFE, 0x7A, 0xF4, 0x00, 0xF5, 0x00, 0x04};
    Nd500Machine m;
    Nd500Cpu cpu;
    setup(&m, &cpu, code, sizeof(code));
    cpu.I[0] = SRC_ADDR;
    cpu.I[1] = DST_ADDR;
    nd500_cpu_step(&cpu);
    check_copied(&m, "F BMOVE copies four 32-bit elements", DST_ADDR);
    nd500_machine_free(&m);
}

int main(void)
{
    printf("ND500 F element string/block move tests\n");
    test_f_smove();
    test_f_smovn();
    test_f_bmove();
    printf("Passed: %d\nFailed: %d\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
