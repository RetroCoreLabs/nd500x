/*
 * PLCCN and ENTIER - two '87 extension instructions, the settled parts only.
 *
 * ND-05.009.4 sections 15.15 (PLCCN) and 10.37 (ENTIER). The unsettled parts
 * (see the headers of src/cpu/instructions/SYSTEM/Plccn.c and
 * src/cpu/instructions/FLOAT_MATH/Entier.c) are not tested here.
 *
 * Every case goes through nd500_cpu_step(), so the opcode table, the decoder
 * and the dispatch table are tested together with the instruction bodies.
 *
 *   FF FD 40 44    W PLCCN b.0,b.4        PLANC descriptor at B+0, result at B+16 (W offsets count words)
 *   FD C7 40 D0    F ENTIER b.0,W1
 *   FD C8 40 D0    D ENTIER b.0,W1
 */

#include <stdio.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/machine/machine_protos.h"

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", name); tests_passed++; } \
    else      { printf("  FAIL: %s\n", name); tests_failed++; } \
} while (0)

#define MEMORY_SIZE (1024 * 1024)
#define CODE_AT     0x1000u
#define DATA_B      0x8000u

static bool run_one(Nd500Machine* m, Nd500Cpu* cpu, const uint8_t* bytes, unsigned n) {
    for (unsigned i = 0; i < n; i++) {
        nd500_bus_write8(m, CODE_AT + i, bytes[i]);
    }
    cpu->B = DATA_B;
    cpu->PC = CODE_AT;
    cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O | ND500_FLAG_C);
    nd500_trap_clear();
    return nd500_cpu_step(cpu);
}

static void plccn_case(Nd500Machine* m, Nd500Cpu* cpu, const char* label,
                       uint32_t a, uint32_t l, uint32_t u,
                       uint32_t want_n, uint32_t want_a, bool want_z) {
    const uint8_t code[] = { 0xFF, 0xFD, 0x40, 0x44 };
    char name[160];
    nd500_bus_write32(m, DATA_B + 0, a);
    nd500_bus_write32(m, DATA_B + 4, l);
    nd500_bus_write32(m, DATA_B + 8, u);
    nd500_bus_write32(m, DATA_B + 16, 0xDEADBEEFu);
    nd500_bus_write32(m, DATA_B + 20, 0xDEADBEEFu);
    (void)run_one(m, cpu, code, 4);
    uint32_t n = nd500_bus_read32(m, DATA_B + 16);
    uint32_t addr = nd500_bus_read32(m, DATA_B + 20);
    snprintf(name, sizeof name, "PLCCN %s: N = %u", label, want_n);
    CHECK(n == want_n, name);
    snprintf(name, sizeof name, "PLCCN %s: A = 0x%X", label, want_a);
    CHECK(addr == want_a, name);
    snprintf(name, sizeof name, "PLCCN %s: Z %s", label, want_z ? "set" : "clear");
    CHECK(((cpu->ST1 & ND500_FLAG_Z) != 0) == want_z, name);
    snprintf(name, sizeof name, "PLCCN %s: four bytes long, no trap", label);
    CHECK(cpu->PC == CODE_AT + 4 && !nd500_trap_occurred(), name);
}

static void entier_case(Nd500Machine* m, Nd500Cpu* cpu, bool dbl, double v,
                        int32_t want, bool want_o) {
    const uint8_t code[] = { 0xFD, dbl ? 0xC8 : 0xC7, 0x40, 0xD0 };
    char name[160];
    bool ov = false, un = false;
    if (dbl) {
        uint64_t bits = nd500_native_double_from_double(v, &ov, &un);
        nd500_bus_write32(m, DATA_B + 0, (uint32_t)(bits >> 32));
        nd500_bus_write32(m, DATA_B + 4, (uint32_t)bits);
    } else {
        nd500_bus_write32(m, DATA_B + 0, nd500_native_single_from_double(v, &ov, &un));
    }
    nd500_write_integer_register(cpu, 1, 0x12345678u);
    (void)run_one(m, cpu, code, 4);
    int32_t got = (int32_t)nd500_read_integer_register(cpu, 1);
    snprintf(name, sizeof name, "%s ENTIER %g -> %d", dbl ? "D" : "F", v, want);
    CHECK(got == want, name);
    snprintf(name, sizeof name, "%s ENTIER %g: Z/S follow the result", dbl ? "D" : "F", v);
    CHECK(((cpu->ST1 & ND500_FLAG_Z) != 0) == (want == 0) &&
          ((cpu->ST1 & ND500_FLAG_S) != 0) == (want < 0), name);
    snprintf(name, sizeof name, "%s ENTIER %g: O %s, no trap", dbl ? "D" : "F", v,
             want_o ? "set" : "clear");
    CHECK(((cpu->ST1 & ND500_FLAG_O) != 0) == want_o && !nd500_trap_occurred() &&
          cpu->PC == CODE_AT + 4, name);
}

int main(void) {
    Nd500Machine m;
    Nd500Cpu cpu;

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    printf("=== PLCCN ===\n");
    plccn_case(&m, &cpu, "a=0x2000 l=1 u=10", 0x2000, 1, 10, 10, 0x2004, false);
    plccn_case(&m, &cpu, "a=0x2000 l=0 u=0", 0x2000, 0, 0, 1, 0x2000, false);
    plccn_case(&m, &cpu, "a=0x3000 l=5 u=4 (empty)", 0x3000, 5, 4, 0, 0x3014, true);
    plccn_case(&m, &cpu, "a=0x3000 l=5 u=2 (negative count)", 0x3000, 5, 2, 0, 0x3014, true);

    printf("=== ENTIER ===\n");
    entier_case(&m, &cpu, false, 2.5, 2, false);
    entier_case(&m, &cpu, false, -2.5, -3, false);
    entier_case(&m, &cpu, false, -2.0, -2, false);
    entier_case(&m, &cpu, false, 0.75, 0, false);
    entier_case(&m, &cpu, false, -0.25, -1, false);
    entier_case(&m, &cpu, true, 7.99, 7, false);
    entier_case(&m, &cpu, true, -7.01, -8, false);
    entier_case(&m, &cpu, true, 3.0e9, INT32_MAX, true);
    entier_case(&m, &cpu, true, -3.0e9, INT32_MIN, true);

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    nd500_machine_free(&m);
    return tests_failed == 0 ? 0 : 1;
}
