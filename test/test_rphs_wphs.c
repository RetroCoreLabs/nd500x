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
#include "../src/cpu/nd500_instructions.h"   /* g_nd500_instrs - the table guard */
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

/* =========================================================================
 * OPERAND ENCODING - the 2026-08-03 regression
 *
 * Until 2026-08-03 both ports marked operand 0 of RPHS/WPHS as O_DIR
 * (0x20000 in op_templates), meaning "four inline literal bytes, no address
 * code". Wrong: it is an ordinary operand. The cost was not a wrong result -
 * it was a wrong LENGTH, so decoding resumed one byte early, INSIDE the
 * operand, and every instruction after it was garbage. That is exactly the
 * SINTRAN swapper's "1 10533B" protect violation.
 *
 * From the swapper P-segment (SWAPPER-K01.PSEG, base 0o1000000000):
 *
 *   1000010525: 377 365 | 304 | 010 001 115 054   rphs <abs 0o1000246454>
 *   1000010534: 300 057                           go   $57
 *
 * 0o304 = 0xC4 = the address code "32-bit absolute address follows". Proven by
 * a sibling in the SAME routine that the disassembler already gets right:
 *
 *   1000010477: 104 304 010 002 075 154   w test $1000436554
 *
 * and 0x08023D6C written in octal IS 0o1000436554.
 *
 * These tests exist because an operand-LENGTH bug is SILENT: the instruction
 * itself still does the right thing, and the damage surfaces as a fault
 * somewhere else entirely.
 * ========================================================================= */

#define ABS_CODE 0xC4u   /* 0o304 - absolute, 32-bit address follows */

/* Lay down opcode + 0o304 + a 32-bit absolute address, and decode it. */
static void build_abs(Nd500Machine* m, Nd500Cpu* cpu, uint16_t opcode,
                      uint32_t operand_addr, Nd500FetchedInstruction* fi) {
    nd500_bus_write8(m, CODE_AT + 0, (uint8_t)(opcode >> 8));
    nd500_bus_write8(m, CODE_AT + 1, (uint8_t)(opcode & 0xFF));
    nd500_bus_write8(m, CODE_AT + 2, (uint8_t)ABS_CODE);
    nd500_bus_write8(m, CODE_AT + 3, (uint8_t)(operand_addr >> 24));
    nd500_bus_write8(m, CODE_AT + 4, (uint8_t)(operand_addr >> 16));
    nd500_bus_write8(m, CODE_AT + 5, (uint8_t)(operand_addr >> 8));
    nd500_bus_write8(m, CODE_AT + 6, (uint8_t)(operand_addr));

    /* The domain number lives IN MEMORY - that is the whole point of a
     * non-direct operand. Read as a direct literal it would be 0xC4xxxxxx,
     * which is not a domain number. */
    nd500_bus_write32(m, operand_addr, 0);        /* domain 0 = CED */

    memset(fi, 0, sizeof(*fi));
    nd500_decode_at(m, CODE_AT, fi);
    cpu->ST1 |= ND500_FLAG_PIA;
    cpu->instr_aborted = 0;
}

/* ---- 6. absolute operand -> the instruction is SEVEN bytes --------------- */
static void test_abs_operand_is_seven_bytes(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 6: RPHS with an absolute operand decodes as 7 bytes\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build_abs(m, cpu, 0xFFF5, 0x3000u, &fi);

    printf("    total_len=%u (want 7)  operand_count=%u (want 1)\n",
           fi.total_len, fi.operand_count);
    CHECK(fi.total_len == 7,     "RPHS abs = 2 opcode + 1 address code + 4 address");
    CHECK(fi.operand_count == 1, "still exactly one operand");

    /* And it must still WORK - a length bug does not stop the move, which is
     * precisely why it went unnoticed for so long. */
    nd500_bus_write8(m, PHYS_SEG_BASE + 600, 0x5A);
    nd500_bus_write8(m, DOM_SEG_BASE + 600, 0x00);
    cpu->I[0] = 1; cpu->I[1] = DOM_VADDR(600); cpu->I[2] = 600; cpu->I[3] = PHYS_SEG_PSN;
    nd500_instr_Rphs(cpu, &fi);
    CHECK(nd500_bus_read8(m, DOM_SEG_BASE + 600) == 0x5A, "the move still happens");
}

/* ---- 7. WPHS carries the identical encoding ------------------------------ */
static void test_wphs_abs_operand_is_seven_bytes(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 7: WPHS with an absolute operand decodes as 7 bytes\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build_abs(m, cpu, 0xFFF4, 0x3000u, &fi);

    printf("    total_len=%u (want 7)\n", fi.total_len);
    CHECK(fi.total_len == 7,
          "WPHS abs = 7 bytes - testing only RPHS would leave half the defect live");
}

/* ---- 8. a short local operand is ONE byte -> 3 in total ------------------ */
static void test_local_operand_is_three_bytes(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 8: RPHS with a local operand (b.24) decodes as 3 bytes\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF5, &fi);   /* ARG_SLOT 0x45 = 0o105 = b.24 */

    printf("    total_len=%u (want 3)\n", fi.total_len);
    CHECK(fi.total_len == 3,
          "proves the fix restored NORMAL operand decoding, not one fixed length for another");
}

/* ---- 9. decoding resumes on the next instruction, not inside the operand -- */
static void test_next_instruction_boundary(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 9: the instruction AFTER an absolute RPHS starts at +7\n");
    Nd500FetchedInstruction fi, next;
    setup_mmu(cpu);
    build_abs(m, cpu, 0xFFF5, 0x3000u, &fi);

    /* A second RPHS at +7, same encoding. With the O_DIR bug decoding resumed
     * at +6 and read two operand bytes as an opcode. */
    nd500_bus_write8(m, CODE_AT +  7, 0xFF);
    nd500_bus_write8(m, CODE_AT +  8, 0xF5);
    nd500_bus_write8(m, CODE_AT +  9, (uint8_t)ABS_CODE);
    nd500_bus_write8(m, CODE_AT + 10, 0x00);
    nd500_bus_write8(m, CODE_AT + 11, 0x00);
    nd500_bus_write8(m, CODE_AT + 12, 0x30);
    nd500_bus_write8(m, CODE_AT + 13, 0x00);

    memset(&next, 0, sizeof(next));
    nd500_decode_at(m, CODE_AT + fi.total_len, &next);

    printf("    next at +%u decodes opcode 0x%04X (want +7 / 0xFFF5)\n",
           fi.total_len, next.opcode);
    CHECK(fi.total_len == 7,     "resume offset is 7");
    CHECK(next.opcode == 0xFFF5, "the follower really is the instruction placed there");
}

/* ---- 10. TABLE GUARD - the defect lived in the table, not the handler ----- */
static void test_table_has_no_direct_operand(void) {
    printf("\nTest 10: neither RPHS nor WPHS may flag an operand as O_DIR\n");
    int checked = 0;
    int clean = 1;
    for (unsigned i = 0; i < g_nd500_instrs_count; i++) {
        if (g_nd500_instrs[i].opcode != 0xFFF5 && g_nd500_instrs[i].opcode != 0xFFF4) continue;
        checked++;
        if (g_nd500_instrs[i].op_templates[0] & 0x20000u) {
            printf("    %s (0x%04X) still has O_DIR: op_templates[0]=0x%08X\n",
                   g_nd500_instrs[i].mnemonic, g_nd500_instrs[i].opcode,
                   g_nd500_instrs[i].op_templates[0]);
            clean = 0;
        }
    }
    CHECK(checked == 2, "both RPHS and WPHS are present in the table");
    /* The cheapest possible regression detector: it fires the moment the tables
     * are regenerated from a stale instructions.json. FOUR copies of that file
     * had to be corrected across the two repositories, and a single missed copy
     * brings the swapper trap straight back. */
    CHECK(clean, "O_DIR (0x20000) is clear - the operand is an ordinary operand");
}

/* =========================================================================
 * EDGE CASES around the page-boundary stop
 * ========================================================================= */

/* ---- 11. ending EXACTLY on a boundary is COMPLETE, not partial ----------- */
static void test_ends_exactly_on_boundary(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 11: a move ending exactly ON a boundary counts as complete\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF5, &fi);

    for (int i = 0; i < 8; i++)
        nd500_bus_write8(m, PHYS_SEG_BASE + 2040 + i, (uint8_t)(0xC0 + i));
    for (int i = 0; i < 8; i++)
        nd500_bus_write8(m, DOM_SEG_BASE + 700 + i, 0);

    cpu->I[0] = 8; cpu->I[1] = DOM_VADDR(700); cpu->I[2] = 2040; cpu->I[3] = PHYS_SEG_PSN;
    cpu->ST1 &= ~ND500_FLAG_Z;

    nd500_instr_Rphs(cpu, &fi);

    int all = 1;
    for (int i = 0; i < 8; i++)
        if (nd500_bus_read8(m, DOM_SEG_BASE + 700 + i) != (uint8_t)(0xC0 + i)) all = 0;

    printf("    I1=%u (want 0)  I3=%u (want 2048)  Z=%d (want 1)\n",
           cpu->I[0], cpu->I[2], (cpu->ST1 & ND500_FLAG_Z) ? 1 : 0);
    CHECK(all,            "every byte moved - the boundary is the END here, not a stop");
    CHECK(cpu->I[0] == 0, "no bytes left");
    /* This separates "stop when I1 = 0" from "stop at a boundary". Report a
     * partial move here and the caller loops forever on a finished transfer. */
    CHECK((cpu->ST1 & ND500_FLAG_Z) != 0,
          "Z set - 'no bytes left' WINS over 'a boundary was reached'");
}

/* ---- 12. starting ON a boundary must still transfer ---------------------- */
static void test_starts_on_boundary(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 12: an I3 already on a boundary still makes progress\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF5, &fi);

    /* PS_AZI is a single page, so offset 0 is the boundary to use here. */
    nd500_bus_write8(m, PHYS_SEG_BASE + 0, 0x40);
    nd500_bus_write8(m, DOM_SEG_BASE + 800, 0x00);

    cpu->I[0] = 1; cpu->I[1] = DOM_VADDR(800); cpu->I[2] = 0; cpu->I[3] = PHYS_SEG_PSN;

    nd500_instr_Rphs(cpu, &fi);

    printf("    I1=%u (want 0)  copied 0x%02X (want 0x40)\n",
           cpu->I[0], nd500_bus_read8(m, DOM_SEG_BASE + 800));
    /* Otherwise a caller resuming a partial move - whose I3 the previous pass
     * left sitting exactly on a boundary - would never make progress. */
    CHECK(cpu->I[0] == 0, "a boundary-aligned start does not stop the move dead");
    CHECK(nd500_bus_read8(m, DOM_SEG_BASE + 800) == 0x40, "the byte moved");
}

/* ---- 13. a single byte - guards the loop-condition off-by-one ------------ */
static void test_single_byte(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 13: RPHS with a count of 1 moves exactly one byte\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF5, &fi);

    nd500_bus_write8(m, PHYS_SEG_BASE + 900, 0x3C);
    nd500_bus_write8(m, PHYS_SEG_BASE + 901, 0xFF);   /* must NOT be copied */
    nd500_bus_write8(m, DOM_SEG_BASE + 900, 0x00);
    nd500_bus_write8(m, DOM_SEG_BASE + 901, 0x00);

    cpu->I[0] = 1; cpu->I[1] = DOM_VADDR(900); cpu->I[2] = 900; cpu->I[3] = PHYS_SEG_PSN;

    nd500_instr_Rphs(cpu, &fi);

    CHECK(nd500_bus_read8(m, DOM_SEG_BASE + 900) == 0x3C, "the one byte moved");
    CHECK(nd500_bus_read8(m, DOM_SEG_BASE + 901) == 0x00, "exactly one - not two");
    CHECK(cpu->I[0] == 0, "I1 reached 0");
}

/* ---- 14. never move MORE than requested ---------------------------------- */
static void test_never_exceeds_count(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 14: RPHS never moves more bytes than I1 asked for\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF5, &fi);

    for (int i = 0; i < 16; i++)
        nd500_bus_write8(m, PHYS_SEG_BASE + 1000 + i, (uint8_t)(0xE0 + i));
    for (int i = 0; i < 16; i++)
        nd500_bus_write8(m, DOM_SEG_BASE + 1000 + i, 0x00);

    cpu->I[0] = 3; cpu->I[1] = DOM_VADDR(1000); cpu->I[2] = 1000; cpu->I[3] = PHYS_SEG_PSN;

    nd500_instr_Rphs(cpu, &fi);

    int first3 = 1;
    for (int i = 0; i < 3; i++)
        if (nd500_bus_read8(m, DOM_SEG_BASE + 1000 + i) != (uint8_t)(0xE0 + i)) first3 = 0;

    CHECK(first3, "the 3 requested bytes moved");
    CHECK(nd500_bus_read8(m, DOM_SEG_BASE + 1003) == 0x00, "byte 4 was never requested");
    CHECK(cpu->I[2] == 1003, "I3 advanced by exactly the requested count");
}

/* ---- 15. WPHS stops on I3 (the segment), not I2 (the domain) ------------- */
static void test_wphs_stops_on_segment_side(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 15: WPHS stops on the PHYSICAL SEGMENT boundary, not the domain address\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF4, &fi);

    /* I3 sits 4 bytes below a boundary; I2 is nowhere near one. If the stop
     * were driven by the domain side, all 64 bytes would move. */
    for (int i = 0; i < 64; i++)
        nd500_bus_write8(m, DOM_SEG_BASE + 1100 + i, (uint8_t)(i + 1));
    for (int i = 0; i < 4; i++)
        nd500_bus_write8(m, PHYS_SEG_BASE + 2044 + i, 0x00);

    cpu->I[0] = 64; cpu->I[1] = DOM_VADDR(1100); cpu->I[2] = 2044; cpu->I[3] = PHYS_SEG_PSN;
    cpu->ST1 |= ND500_FLAG_Z;

    nd500_instr_Wphs(cpu, &fi);

    printf("    I1=%u (want 60)  I3=%u (want 2048)  Z=%d (want 0)\n",
           cpu->I[0], cpu->I[2], (cpu->ST1 & ND500_FLAG_Z) ? 1 : 0);
    CHECK(cpu->I[0] == 60,                "the stop is driven by I3, not I2");
    CHECK(cpu->I[2] == 2048,              "I3 stops on the boundary");
    CHECK((cpu->ST1 & ND500_FLAG_Z) == 0, "Z cleared - bytes still left");
}

/* =========================================================================
 * NEGATIVE TESTS - what must NOT happen
 * ========================================================================= */

/* ---- 16. unprivileged RPHS must trap and move NOTHING -------------------- */
static void test_rphs_requires_privilege(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 16: an unprivileged RPHS traps and moves nothing\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF5, &fi);

    nd500_bus_write8(m, PHYS_SEG_BASE + 1200, 0x99);
    nd500_bus_write8(m, DOM_SEG_BASE + 1200, 0x00);

    cpu->ST1 &= ~ND500_FLAG_PIA;        /* deliberately NOT privileged */
    cpu->I[0] = 4; cpu->I[1] = DOM_VADDR(1200); cpu->I[2] = 1200; cpu->I[3] = PHYS_SEG_PSN;

    nd500_instr_Rphs(cpu, &fi);

    /* A privilege check that traps but copies anyway is worse than none at all:
     * it leaks physical memory to an unprivileged domain while the trap log
     * still looks correct. */
    CHECK(nd500_bus_read8(m, DOM_SEG_BASE + 1200) == 0x00, "not one byte transferred");
    CHECK(cpu->I[0] == 4, "registers untouched - nothing moved, so nothing counts down");

    cpu->ST1 |= ND500_FLAG_PIA;
    cpu->instr_aborted = 0;
}

/* ---- 17. unprivileged WPHS - the more dangerous direction ---------------- */
static void test_wphs_requires_privilege(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\nTest 17: an unprivileged WPHS traps and writes nothing\n");
    Nd500FetchedInstruction fi;
    setup_mmu(cpu);
    build(m, cpu, 0xFFF4, &fi);

    nd500_bus_write8(m, DOM_SEG_BASE + 1300, 0x66);
    nd500_bus_write8(m, PHYS_SEG_BASE + 1300, 0x00);

    cpu->ST1 &= ~ND500_FLAG_PIA;
    cpu->I[0] = 4; cpu->I[1] = DOM_VADDR(1300); cpu->I[2] = 1300; cpu->I[3] = PHYS_SEG_PSN;

    nd500_instr_Wphs(cpu, &fi);

    CHECK(nd500_bus_read8(m, PHYS_SEG_BASE + 1300) == 0x00,
          "an unprivileged WPHS must not write INTO a physical segment");
    CHECK(cpu->I[0] == 4, "registers untouched");

    cpu->ST1 |= ND500_FLAG_PIA;
    cpu->instr_aborted = 0;
}

/* ---- the PSN-rooted walk must read the SAME table as the virtual walk ------
 *
 * RPHS/WPHS translate rooted at a physical segment number instead of a
 * capability, but the physical segment TABLE they consult is the same one. Ours
 * used a second predicate to decide which table to read - the mmu_guest_tables
 * setting, which defaults off - so once a guest had published a PST, every
 * ordinary translation read the guest's table while RPHS still read the
 * emulator's shadow.
 *
 * Measured on SINTRAN's ND-500/5000 MONITOR J04: loading the swapper, an RPHS
 * for physical segment 10 reported "PST entry 10 is ZERO" with use_guest=0,
 * while the guest table at PSTP held 0x00000118 for that entry - both read paths
 * agreeing on the value and the walk looking somewhere else. The monitor turned
 * the bogus page fault into "NOT KNOWN TRAP" and a FATAL.
 *
 * So: publish a guest PST that maps a PSN, leave the SHADOW entry for that same
 * PSN zero, and require RPHS to find the mapping. With the old predicate it
 * finds the zero shadow entry and faults.
 */
#define GUEST_PSTP     0x30000u                  /* where the "guest" publishes its PST */
#define GUEST_ONLY_PSN 10                        /* the live case's segment number */
#define GUEST_ONLY_PFN 0x118u                    /* the live case's entry value */

static void test_psn_walk_uses_the_guest_pst(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n-- the PSN-rooted walk reads the guest PST, not the shadow --\n");

    setup_mmu(cpu);

    /* The shadow entry for this PSN stays ZERO - nd500_mmu_init cleared it and
     * setup_mmu only sets the other two. State it rather than assume it. */
    PhysicalSegmentTableEntry shadow = nd500_mmu_get_pst_entry(cpu, GUEST_ONLY_PSN);
    CHECK(shadow.index_mode == PS_AZI && shadow.physical_pfn == 0,
          "shadow PST entry for the PSN is zero");

    /* Publish the guest table: 32-bit entries at PSTP, index mode in bits 31-30
     * and the page frame in bits 29-0. That is the ND-5000 width, measured on
     * SINTRAN III L over the octobus; the halfword form belongs to the older
     * ND500 generation and is not what this lane uses. */
    nd500_bus_write32(m, GUEST_PSTP + (uint32_t)GUEST_ONLY_PSN * 4u,
                      ((uint32_t)PS_AZI << 30) | GUEST_ONLY_PFN);
    cpu->PSTP = GUEST_PSTP;
    cpu->dit_configured = 1;

    /* A byte the walk should be able to reach, written through the physical
     * address the guest entry implies. */
    const uint32_t expect_phys = GUEST_ONLY_PFN * NBPG + 0x24u;
    nd500_bus_write8(m, expect_phys, 0x5A);

    nd500_trap_clear();
    cpu->instr_aborted = 0;
    uint32_t got = nd500_mmu_translate_physical_segment(cpu, GUEST_ONLY_PSN, 0x24u, 0);

    CHECK(!nd500_trap_occurred() && !cpu->instr_aborted,
          "no trap - the guest entry maps the segment");
    CHECK(got == expect_phys, "translated through the guest PST entry");
    CHECK(nd500_bus_read8(m, got) == 0x5A, "and the byte there is the one written");

    /* Put the CPU back as the other cases expect it: no published guest table. */
    cpu->PSTP = 0;
    cpu->dit_configured = 0;
    nd500_trap_clear();
    cpu->instr_aborted = 0;
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

    /* Operand encoding - the 2026-08-03 regression. */
    test_abs_operand_is_seven_bytes(&m, &cpu);
    test_wphs_abs_operand_is_seven_bytes(&m, &cpu);
    test_local_operand_is_three_bytes(&m, &cpu);
    test_next_instruction_boundary(&m, &cpu);
    test_table_has_no_direct_operand();
    test_psn_walk_uses_the_guest_pst(&m, &cpu);

    /* Edge cases around the page-boundary stop. */
    test_ends_exactly_on_boundary(&m, &cpu);
    test_starts_on_boundary(&m, &cpu);
    test_single_byte(&m, &cpu);
    test_never_exceeds_count(&m, &cpu);
    test_wphs_stops_on_segment_side(&m, &cpu);

    /* Negative tests. */
    test_rphs_requires_privilege(&m, &cpu);
    test_wphs_requires_privilege(&m, &cpu);

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    nd500_machine_free(&m);
    return tests_failed == 0 ? 0 : 1;
}
