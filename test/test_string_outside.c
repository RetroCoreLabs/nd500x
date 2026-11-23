/**
 * ND-05.009.4 14.1 ADDRESSING OUTSIDE STRINGS for the string move instructions
 * SMOVE (14.2), SMVWH (14.3), SMVUN (14.4), SMVTR (14.5), SMVTU (14.6).
 *
 * 14.1: "If the pointer register points outside the string when the instruction
 * starts execution, a descriptor range trap condition arises ... Addressing a
 * string of length zero will always be outside the string." "no string elements
 * will be examined, moved, or compared. The I1 and I2 registers are then
 * unmodified". Each instruction page lists: outside source K=0 Z=0, outside
 * dest K=1 Z=0, I1, I2 unmodified, DR trap condition.
 *
 * DR is trap condition bit 25 (ignorable, manual 6.5.3); not enabled here, so
 * it shows as the status bit in ST1.
 *
 * Layout: B = 0x2000, source descriptor at B.100, destination descriptor at
 * B.200 (ND-05.009.4 7.2.8: element count, then address), translation table at
 * B.40. Source bytes 1..4 at 0x4000, destination bytes 0 at 0x5000.
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
#define ST1_DR    (1u << 25)

static int tests_passed = 0;
static int tests_failed = 0;

static void put_be32(Nd500Machine *m, uint32_t addr, uint32_t v)
{
    nd500_bus_write8(m, addr + 0u, (uint8_t)(v >> 24));
    nd500_bus_write8(m, addr + 1u, (uint8_t)(v >> 16));
    nd500_bus_write8(m, addr + 2u, (uint8_t)(v >> 8));
    nd500_bus_write8(m, addr + 3u, (uint8_t)v);
}

static void run_case(const char *name, const uint8_t *code, uint32_t len,
                     uint32_t src_count, uint32_t i1, uint32_t i2, bool expect_k)
{
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    for (uint32_t i = 0u; i < 4u; i++) {
        nd500_bus_write8(&m, SRC_ADDR + i, (uint8_t)(i + 1u));
        nd500_bus_write8(&m, DST_ADDR + i, 0u);
    }
    /* identity translation table, so a move would be visible */
    for (uint32_t i = 0u; i < 256u; i++) {
        nd500_bus_write8(&m, B_ADDR + 40u + i, (uint8_t)i);
    }
    put_be32(&m, B_ADDR + 100u, src_count);
    put_be32(&m, B_ADDR + 104u, SRC_ADDR);
    put_be32(&m, B_ADDR + 200u, 4u);
    put_be32(&m, B_ADDR + 204u, DST_ADDR);
    for (uint32_t i = 0u; i < len; i++) {
        nd500_bus_write8(&m, CODE_ADDR + i, code[i]);
    }
    cpu.PC = CODE_ADDR;
    cpu.B = B_ADDR;
    cpu.I[0] = i1;
    cpu.I[1] = i2;
    cpu.ST1 = 0u;
    m.run_flag = 1;

    nd500_cpu_step(&cpu);

    bool moved = false;
    for (uint32_t i = 0u; i < 4u; i++) {
        if (nd500_bus_read8(&m, DST_ADDR + i) != 0u) {
            moved = true;
        }
    }
    bool k = nd500_test_flag(&cpu, ND500_FLAG_K);
    bool z = nd500_test_flag(&cpu, ND500_FLAG_Z);
    bool dr = (cpu.ST1 & ST1_DR) != 0u;
    if (!moved && k == expect_k && !z && dr && cpu.I[0] == i1 && cpu.I[1] == i2) {
        printf("  [PASS] %s\n", name);
        tests_passed++;
    } else {
        printf("  [FAIL] %s: moved=%d K=%d (want %d) Z=%d DR=%d I1=%u I2=%u\n",
               name, moved, k, expect_k, z, dr, cpu.I[0], cpu.I[1]);
        tests_failed++;
    }
    nd500_machine_free(&m);
}

int main(void)
{
    /* BY SMOVE B.100, B.200 (0FD67H) */
    static const uint8_t smove[] = {0xFD, 0x67, 0x59, 0x72};
    /* BY SMVWH B.100, B.200, 0, 0 (0FD72H): mask 0, test 0 - every byte passes */
    static const uint8_t smvwh[] = {0xFD, 0x72, 0x59, 0x72, 0x00, 0x00};
    /* BY SMVUN B.100, B.200, 0, 1 (0FD73H): AND with 0 is never 1 - no stop byte */
    static const uint8_t smvun[] = {0xFD, 0x73, 0x59, 0x72, 0x00, 0x01};
    /* BY SMVTR B.100, B.200, B.40 (0FD74H) */
    static const uint8_t smvtr[] = {0xFD, 0x74, 0x59, 0x72, 0x4A};
    /* BY SMVTU B.100, B.200, B.40 (0FD75H) */
    static const uint8_t smvtu[] = {0xFD, 0x75, 0x59, 0x72, 0x4A};
    static const struct { const char *name; const uint8_t *code; uint32_t len; } ins[] = {
        {"SMOVE", smove, sizeof smove}, {"SMVWH", smvwh, sizeof smvwh},
        {"SMVUN", smvun, sizeof smvun}, {"SMVTR", smvtr, sizeof smvtr},
        {"SMVTU", smvtu, sizeof smvtu},
    };

    printf("ND500 string move: addressing outside strings (ND-05.009.4 14.1)\n");
    for (size_t n = 0; n < sizeof ins / sizeof ins[0]; n++) {
        char name[96];
        snprintf(name, sizeof name, "%s outside dest (I2 = length) K=1", ins[n].name);
        run_case(name, ins[n].code, ins[n].len, 4u, 0u, 4u, true);
        snprintf(name, sizeof name, "%s outside source (I1 = length) K=0", ins[n].name);
        run_case(name, ins[n].code, ins[n].len, 4u, 4u, 0u, false);
        snprintf(name, sizeof name, "%s source of length zero K=0", ins[n].name);
        run_case(name, ins[n].code, ins[n].len, 0u, 0u, 0u, false);
    }
    printf("Passed: %d\nFailed: %d\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
