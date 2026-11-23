/*
 * ND-05.009.4 14.1 ADDRESSING OUTSIDE STRINGS for the string instructions that
 * examine but do not move: SCOMP (14.10), SCOTR (14.11), SSKIP (14.14),
 * SLOCA (14.15), SSCAN (14.16), SSPAN (14.17), SMATCH (14.18), SCHPAR (14.20).
 *
 * 14.1: "If any string operand is addressed outside its legal range, no string
 * elements will be examined, moved, or compared. The I1 and I2 registers are
 * then unmodified, and a descriptor-range trap condition occurs." "Addressing
 * a string of length zero will always be outside the string." The status each
 * instruction leaves comes from its own terminating conditions:
 *   14.10/14.11: both outside K=0 Z=1 S=0; source-1 outside compares as
 *     "source-1 shorter" (K=0 Z=0 S=1); source-2 outside as "source-1 longer"
 *     (K=0 Z=0 S=0).
 *   14.14 SSKIP K=0 Z=1 S=0; 14.15 SLOCA K=0 Z=1; 14.16 SSCAN K=0 Z=1;
 *   14.17 SSPAN K=0 Z=0; 14.20 SCHPAR K=0 Z=0.
 *   14.18 SMATCH: outside substring (or both) K=0 Z=1; outside string with the
 *     substring inside K=0 Z=0.
 *
 * DR is trap condition bit 25 (ignorable); not enabled here, so it shows as
 * the status bit in ST1. The same cases are in RetroCore
 * TestND500_StringExamineOutside.
 *
 * Layout: B = 0x2000, operand-1 descriptor at B.100 (count, then address
 * 0x4000), operand-2 descriptor at B.200 (count 4, address 0x5000), identity
 * translation table at B.208 (256 bytes from 0x20D0, clear of both
 * descriptors). Bytes 1..4 at 0x4000 and at 0x5000.
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
                     uint32_t first_count, uint32_t i1, uint32_t i2,
                     bool want_z, bool want_s)
{
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    for (uint32_t i = 0u; i < 256u; i++) {
        nd500_bus_write8(&m, B_ADDR + 208u + i, (uint8_t)i);
    }
    for (uint32_t i = 0u; i < 4u; i++) {
        nd500_bus_write8(&m, SRC_ADDR + i, (uint8_t)(i + 1u));
        nd500_bus_write8(&m, DST_ADDR + i, (uint8_t)(i + 1u));
    }
    put_be32(&m, B_ADDR + 100u, first_count);
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

    bool k = nd500_test_flag(&cpu, ND500_FLAG_K);
    bool z = nd500_test_flag(&cpu, ND500_FLAG_Z);
    bool s = nd500_test_flag(&cpu, ND500_FLAG_S);
    bool dr = (cpu.ST1 & ST1_DR) != 0u;
    if (!k && z == want_z && s == want_s && dr && cpu.I[0] == i1 && cpu.I[1] == i2) {
        printf("  [PASS] %s\n", name);
        tests_passed++;
    } else {
        printf("  [FAIL] %s: K=%d Z=%d (want %d) S=%d (want %d) DR=%d I1=%u I2=%u\n",
               name, k, z, want_z, s, want_s, dr, cpu.I[0], cpu.I[1]);
        tests_failed++;
    }
    nd500_machine_free(&m);
}

int main(void)
{
    static const uint8_t scomp[] = {0xFD, 0xAC, 0x59, 0x72};        /* BY SCOMP B.100, B.200 */
    static const uint8_t scotr[] = {0xFD, 0xAD, 0x59, 0x72, 0x74};  /* BY SCOTR B.100, B.200, B.208 */
    static const uint8_t sskip[] = {0xFD, 0xAE, 0x59, 0x00};        /* BY SSKIP B.100, 0 */
    static const uint8_t sloca[] = {0xFD, 0xB0, 0x59, 0x00};        /* BY SLOCA B.100, 0 */
    static const uint8_t sscan[] = {0xFD, 0xB1, 0x59, 0x01, 0x74};  /* BY SSCAN B.100, 1, B.208 */
    static const uint8_t sspan[] = {0xFD, 0xB2, 0x59, 0x01, 0x74};  /* BY SSPAN B.100, 1, B.208 */
    static const uint8_t smatch[] = {0xFD, 0xB3, 0x59, 0x72};       /* BY SMATCH B.100, B.200 */
    static const uint8_t schpar[] = {0xFD, 0xB5, 0x59, 0x00};       /* BY SCHPAR B.100, 0 */

    printf("ND500 string examine: addressing outside strings (ND-05.009.4 14.1)\n");
    run_case("SCOMP both outside", scomp, sizeof scomp, 4u, 4u, 4u, true, false);
    run_case("SCOMP source-1 outside", scomp, sizeof scomp, 4u, 4u, 0u, false, true);
    run_case("SCOMP source-2 outside", scomp, sizeof scomp, 4u, 0u, 4u, false, false);
    run_case("SCOMP zero-length source-1", scomp, sizeof scomp, 0u, 0u, 0u, false, true);
    run_case("SCOTR both outside", scotr, sizeof scotr, 4u, 4u, 4u, true, false);
    run_case("SCOTR source-1 outside", scotr, sizeof scotr, 4u, 4u, 0u, false, true);
    run_case("SCOTR source-2 outside", scotr, sizeof scotr, 4u, 0u, 4u, false, false);
    run_case("SSKIP outside source", sskip, sizeof sskip, 4u, 4u, 0u, true, false);
    run_case("SSKIP zero-length source", sskip, sizeof sskip, 0u, 0u, 0u, true, false);
    run_case("SLOCA outside source", sloca, sizeof sloca, 4u, 4u, 0u, true, false);
    run_case("SSCAN outside source", sscan, sizeof sscan, 4u, 4u, 0u, true, false);
    run_case("SSPAN outside source", sspan, sizeof sspan, 4u, 4u, 0u, false, false);
    run_case("SMATCH outside substring", smatch, sizeof smatch, 4u, 4u, 0u, true, false);
    run_case("SMATCH outside string", smatch, sizeof smatch, 4u, 0u, 4u, false, false);
    run_case("SCHPAR outside string", schpar, sizeof schpar, 4u, 4u, 0u, false, false);
    printf("Passed: %d\nFailed: %d\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
