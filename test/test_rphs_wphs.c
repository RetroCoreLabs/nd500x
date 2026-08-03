/*
 * RPHS / WPHS - read from / write to a PHYSICAL SEGMENT.
 * ND-05.009.4 EN, sections 16.31 (RPHS, 0xFFF5) and 16.32 (WPHS, 0xFFF4).
 *
 * These two instructions were unreachable: the decoder emits ONE operand
 * (nd500_instructions.c: { 0xFFF5, "rphs", 1, ... }) while both handlers
 * demanded three, so the guard called trap_illegal_operand on every single
 * execution. Underneath that, five defects: the source came from operand 0
 * instead of [I4,I3), I3 and I4 were never read, there was no physical-segment
 * translation at all (a plain memory-to-memory copy in the current domain), no
 * page-boundary stop, and I1 = 0 / Z = 1 were set unconditionally while I3 was
 * never updated.
 *
 * The manual's operation, quoted:
 *
 *     while I1 > 0 do
 *       S([I4,I3) -> D(<domain number>.I2)     (RPHS; reversed for WPHS)
 *       I3 + 1 -> I3;  I2 + 1 -> I2;  I1 - 1 -> I1
 *     enddo
 *
 * "The copy operation is continued until the number of bytes left is equal to
 *  0 (I1 = 0) or a page boundary is reached on the physical segment."
 *
 * So it is a PARTIAL move that the caller loops on, and the registers are the
 * interface. THE PAGE-BOUNDARY TEST IS THE ONE THAT MATTERS: an implementation
 * that simply copies the whole count passes every other test in this file.
 *
 * Each test asserts on bytes that actually moved in emulator memory, not on
 * what the instruction says it did.
 */

#include <stdio.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/machine/machine_protos.h"

extern void nd500_instr_Rphs(Nd500Cpu*, const Nd500FetchedInstruction*);
extern void nd500_instr_Wphs(Nd500Cpu*, const Nd500FetchedInstruction*);

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", name); tests_passed++; } \
    else      { printf("  FAIL: %s\n", name); tests_failed++; } \
} while (0)

#define MEMORY_SIZE   (8 * 1024 * 1024)
#define CODE_AT       0x1000u

/* The physical segment under test. PS_AZI is a single 2 KB page whose physical
 * page comes straight from the PST entry, which makes both the mapping and the
 * page boundary trivial to reason about: segment offset 0..2047 is valid and
 * 2048 is the boundary. */
#define PHYS_SEG_PSN  200
#define PHYS_SEG_PFN  0x40u                      /* physical 0x20000 */
#define PHYS_SEG_BASE (PHYS_SEG_PFN * NBPG)

/* The domain side. Segment 1 (SGSHIFT = 27), also PS_AZI, mapped writable. */
#define DOM_SEG        1
#define DOM_SEG_PSN    201
#define DOM_SEG_PFN    0x50u                     /* physical 0x28000 */
#define DOM_SEG_BASE   (DOM_SEG_PFN * NBPG)
#define DOM_VADDR(off) (((uint32_t)DOM_SEG << SGSHIFT) | (uint32_t)(off))

/* RPHS/WPHS take ONE operand: the domain number. Operand byte 0x45 = b.0x14,
 * the first argument slot, matching how the kernel passes a frame argument. */
#define ARG_SLOT      0x45
#define FRAME_B       0x100000u

static void build(Nd500Machine* m, Nd500Cpu* cpu, uint16_t opcode, Nd500FetchedInstruction* fi) {
    nd500_bus_write8(m, CODE_AT + 0, (uint8_t)(opcode >> 8));
    nd500_bus_write8(m, CODE_AT + 1, (uint8_t)(opcode & 0xFF));
    nd500_bus_write8(m, CODE_AT + 2, ARG_SLOT);

    cpu->B = FRAME_B;
    nd500_bus_write32(m, FRAME_B + 0x14, 0);     /* domain number = 0 = CED */

    memset(fi, 0, sizeof(*fi));
    nd500_decode_at(m, CODE_AT, fi);
    cpu->ST1 |= ND500_FLAG_PIA;                  /* both are privileged */
    cpu->instr_aborted = 0;
}

static void setup_mmu(Nd500Cpu* cpu) {
    nd500_mmu_init(cpu);
    nd500_mmu_enable(cpu);
    /* Physical segment: reachable ONLY by PSN, never by a capability - that is
     * the whole point of RPHS/WPHS. */
    nd500_mmu_set_pst_entry(cpu, PHYS_SEG_PSN, PS_AZI, PHYS_SEG_PFN);
    /* Domain-side segment, reached the ordinary way. */
    nd500_mmu_set_pst_entry(cpu, DOM_SEG_PSN, PS_AZI, DOM_SEG_PFN);
    nd500_mmu_set_data_capability(cpu, 0, DOM_SEG, (uint16_t)(DOM_SEG_PSN | DC_WRP));
    nd500_mmu_set_program_capability(cpu, 0, DOM_SEG, (uint16_t)DOM_SEG_PSN);
}

/* ---- 1. the operand-count guard no longer rejects the instruction --------- */
static void test_accepts_single_operand(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 1: RPHS accepts its single domain operand and copies\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF5, &fi);

    const char* payload = "PHYS";
    for (int i = 0; i < 4; i++)
        nd500_bus_write8(m, PHYS_SEG_BASE + i, (uint8_t)payload[i]);
    for (int i = 0; i < 4; i++)
        nd500_bus_write8(m, DOM_SEG_BASE + i, 0);

    cpu->I[0] = 4;                     /* I1 byte count               */
    cpu->I[1] = DOM_VADDR(0);          /* I2 domain logical address   */
    cpu->I[2] = 0;                     /* I3 offset on the segment    */
    cpu->I[3] = PHYS_SEG_PSN;          /* I4 physical segment number  */

    nd500_instr_Rphs(cpu, &fi);

    char got[5] = {0};
    for (int i = 0; i < 4; i++) got[i] = (char)nd500_bus_read8(m, DOM_SEG_BASE + i);
    printf("    domain memory now \"%s\" (want \"PHYS\")\n", got);
    CHECK(memcmp(got, payload, 4) == 0, "RPHS copies segment -> domain");
}

/* ---- 2. I1 -> 0, I2 and I3 advance, Z set -------------------------------- */
static void test_updates_registers_and_z(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 2: RPHS updates I1/I2/I3 and sets Z on completion\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF5, &fi);

    for (int i = 0; i < 8; i++) nd500_bus_write8(m, PHYS_SEG_BASE + 16 + i, (uint8_t)(0xA0 + i));

    cpu->I[0] = 8;
    cpu->I[1] = DOM_VADDR(32);
    cpu->I[2] = 16;
    cpu->I[3] = PHYS_SEG_PSN;
    cpu->ST1 &= ~ND500_FLAG_Z;

    nd500_instr_Rphs(cpu, &fi);

    printf("    I1=%u (want 0)  I2=0x%08X (want 0x%08X)  I3=%u (want 24)  Z=%d (want 1)\n",
           cpu->I[0], cpu->I[1], DOM_VADDR(40), cpu->I[2],
           (cpu->ST1 & ND500_FLAG_Z) ? 1 : 0);
    CHECK(cpu->I[0] == 0,                       "I1 counted down to 0");
    CHECK(cpu->I[1] == DOM_VADDR(40),           "I2 advanced by the byte count");
    CHECK(cpu->I[2] == 24,                      "I3 advanced - it was never updated before");
    CHECK((cpu->ST1 & ND500_FLAG_Z) != 0,       "Z set on completion");
}

/* ---- 3. THE ONE THAT MATTERS: stop at a page boundary, clear Z ------------ */
static void test_stops_at_page_boundary(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 3: RPHS stops at a page boundary on the physical segment\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF5, &fi);

    /* Start 4 bytes below the 2048 boundary and ask for 64. Exactly 4 may move. */
    for (int i = 0; i < 4; i++)
        nd500_bus_write8(m, PHYS_SEG_BASE + 2044 + i, (uint8_t)(0xE0 + i));
    for (int i = 0; i < 64; i++)
        nd500_bus_write8(m, DOM_SEG_BASE + 64 + i, 0x00);

    cpu->I[0] = 64;
    cpu->I[1] = DOM_VADDR(64);
    cpu->I[2] = 2044;
    cpu->I[3] = PHYS_SEG_PSN;
    cpu->ST1 |= ND500_FLAG_Z;          /* start SET, so clearing it is observable */

    nd500_instr_Rphs(cpu, &fi);

    int moved_ok = 1;
    for (int i = 0; i < 4; i++)
        if (nd500_bus_read8(m, DOM_SEG_BASE + 64 + i) != (uint8_t)(0xE0 + i)) moved_ok = 0;
    int past_clean = 1;
    for (int i = 4; i < 64; i++)
        if (nd500_bus_read8(m, DOM_SEG_BASE + 64 + i) != 0x00) past_clean = 0;

    printf("    I1=%u (want 60)  I3=%u (want 2048)  Z=%d (want 0)\n",
           cpu->I[0], cpu->I[2], (cpu->ST1 & ND500_FLAG_Z) ? 1 : 0);
    CHECK(moved_ok,                             "the 4 bytes below the boundary moved");
    CHECK(past_clean,                           "NOTHING was copied past the boundary");
    CHECK(cpu->I[0] == 60,                      "I1 left at 60 - the move is partial");
    CHECK(cpu->I[2] == 2048,                    "I3 left exactly ON the boundary");
    CHECK((cpu->ST1 & ND500_FLAG_Z) == 0,       "Z CLEARED - caller must loop again");
}

/* ---- 4. the reverse direction -------------------------------------------- */
static void test_wphs_domain_to_segment(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 4: WPHS copies domain -> physical segment\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF4, &fi);

    const char* payload = "DOM!";
    for (int i = 0; i < 4; i++) nd500_bus_write8(m, DOM_SEG_BASE + 128 + i, (uint8_t)payload[i]);
    for (int i = 0; i < 4; i++) nd500_bus_write8(m, PHYS_SEG_BASE + 256 + i, 0);

    cpu->I[0] = 4;
    cpu->I[1] = DOM_VADDR(128);
    cpu->I[2] = 256;
    cpu->I[3] = PHYS_SEG_PSN;

    nd500_instr_Wphs(cpu, &fi);

    char got[5] = {0};
    for (int i = 0; i < 4; i++) got[i] = (char)nd500_bus_read8(m, PHYS_SEG_BASE + 256 + i);
    printf("    physical segment now \"%s\" (want \"DOM!\")  I1=%u (want 0)\n", got, cpu->I[0]);
    CHECK(memcmp(got, payload, 4) == 0, "WPHS copies domain -> segment");
    CHECK(cpu->I[0] == 0,               "WPHS counted I1 down to 0");
}

/* ---- 5. `while I1 > 0` means an empty move is a COMPLETED move ------------ */
static void test_zero_count(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 5: RPHS with a zero count moves nothing and sets Z\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF5, &fi);

    nd500_bus_write8(m, DOM_SEG_BASE + 512, 0x5A);

    cpu->I[0] = 0;
    cpu->I[1] = DOM_VADDR(512);
    cpu->I[2] = 0;
    cpu->I[3] = PHYS_SEG_PSN;
    cpu->ST1 &= ~ND500_FLAG_Z;

    nd500_instr_Rphs(cpu, &fi);

    uint8_t untouched = nd500_bus_read8(m, DOM_SEG_BASE + 512);
    printf("    domain byte still 0x%02X (want 0x5A)  I3=%u (want 0)  Z=%d (want 1)\n",
           untouched, cpu->I[2], (cpu->ST1 & ND500_FLAG_Z) ? 1 : 0);
    CHECK(untouched == 0x5A,              "nothing was copied");
    CHECK(cpu->I[2] == 0,                 "I3 did not move");
    CHECK((cpu->ST1 & ND500_FLAG_Z) != 0, "Z set - an empty move is a completed move");
}

int main(void) {
    Nd500Machine m;
    Nd500Cpu cpu;

    printf("=== RPHS / WPHS - physical segment move (16.31 / 16.32) ===\n");

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    test_accepts_single_operand(&m, &cpu);
    test_updates_registers_and_z(&m, &cpu);
    test_stops_at_page_boundary(&m, &cpu);
    test_wphs_domain_to_segment(&m, &cpu);
    test_zero_count(&m, &cpu);

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    nd500_machine_free(&m);
    return tests_failed == 0 ? 0 : 1;
}
