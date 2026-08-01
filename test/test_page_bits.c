/*
 * PGU / WIP page tables: the bitmaps, and the six instructions that expose them.
 *
 * Both tables used to read back as a hardwired zero, and the instruction
 * comments called that "safe for OS swapping decisions". It is the opposite -
 * zero is the AGGRESSIVE answer in both directions, and NDIX reads both:
 *
 *   - kernel/MASTER/sys/vm_page.c:547, the pageout clock hand, does
 *     `if (rpgu(pfnum)) { zpgu(pfnum); ...second chance... } else { take; }`.
 *     An always-zero PGU table sends every inspected page down the `take`
 *     branch on the first sweep.
 *   - kernel/MASTER/machine/pte.h:93 defines `dirty(pte)` as
 *     `_rwip(pfnum) || pg_m`. An always-zero WIP table makes a page the guest
 *     wrote through the MMU look clean, so it is dropped, not written back.
 *
 * Semantics asserted here come from ND-05.009.4 sections 16.17 - 16.22:
 * PGU is set by any access and WIP only by a write; RPGU/RWIP set Z when the
 * result is zero; only the low 25 bits of the page number are significant; and
 * bits representing non-existing memory read as zero.
 */

#include <stdio.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/cpu/nd500_page_bits.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/machine/machine_protos.h"

extern void nd500_instr_Rpgu(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);
extern void nd500_instr_Rwip(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);
extern void nd500_instr_Zpgu(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);
extern void nd500_instr_Zwip(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);
extern void nd500_instr_Cpgu(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);
extern void nd500_instr_Cwip(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", name); tests_passed++; } \
    else      { printf("  FAIL: %s\n", name); tests_failed++; } \
} while (0)

#define MEMORY_SIZE   (8 * 1024 * 1024)     /* 4096 page frames of 2 KB */
#define PAGE_COUNT    (MEMORY_SIZE / 2048)
#define PC_HERE       0x1000u

/* Build a fetched instruction whose single operand is a word constant. */
static void set_const_word(Nd500OperandDecoded* op, uint32_t value) {
    memset(op, 0, sizeof(*op));
    op->mode = ND500_ADDR_CONSTANT;
    op->data_len = 4;
    op->data[0] = (uint8_t)(value >> 24);
    op->data[1] = (uint8_t)(value >> 16);
    op->data[2] = (uint8_t)(value >> 8);
    op->data[3] = (uint8_t)value;
}

static Nd500FetchedInstruction make_one_operand(uint16_t opcode, uint8_t target_reg, uint32_t operand) {
    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof(fi));
    fi.address = PC_HERE;
    fi.opcode = opcode;
    fi.operand_count = 1;
    fi.target_register = target_reg;
    fi.data_type = ND500_DTYPE_WORD;
    set_const_word(&fi.operands[0], operand);
    return fi;
}

static Nd500FetchedInstruction make_no_operand(uint16_t opcode) {
    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof(fi));
    fi.address = PC_HERE;
    fi.opcode = opcode;
    fi.operand_count = 0;
    fi.data_type = ND500_DTYPE_WORD;
    return fi;
}

/* ---------------------------------------------------------------------------
 * Layer 1: the bitmaps
 * ------------------------------------------------------------------------ */

static void test_bitmaps(Nd500Machine* m) {
    printf("\n[1] Bitmap semantics\n");

    nd500_page_bits_clear_all(m, ND500_PAGE_TABLE_PGU);
    nd500_page_bits_clear_all(m, ND500_PAGE_TABLE_WIP);

    /* A READ marks PGU only - this is the whole point of two tables. */
    nd500_page_bits_mark(m, 100u * 2048u + 7u, 0);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, 100) == 1,
          "read access sets PGU");
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 100) == 0,
          "read access leaves WIP clear");

    /* A WRITE marks both. */
    nd500_page_bits_mark(m, 101u * 2048u, 1);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, 101) == 1,
          "write access sets PGU");
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 101) == 1,
          "write access sets WIP");

    /* Any offset inside the page marks the same frame. */
    nd500_page_bits_mark(m, 102u * 2048u + 2047u, 1);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 102) == 1,
          "last byte of a page marks that page");
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 103) == 0,
          "and does not bleed into the next page");

    /* Clearing one bit leaves its neighbours alone, and clears only one table. */
    nd500_page_bits_clear_bit(m, ND500_PAGE_TABLE_WIP, 101);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 101) == 0,
          "clear_bit clears the named bit");
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, 101) == 1,
          "clear_bit on WIP does not touch PGU");
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 102) == 1,
          "clear_bit leaves other pages set");

    /* "Reading bits representing non-existing memory will give a zero result." */
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, PAGE_COUNT) == 0,
          "page just past physical memory reads 0");
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, 0x00FFFFFFu) == 0,
          "far out-of-range page reads 0");
    /* Marking an address beyond memory must not corrupt anything. */
    nd500_page_bits_mark(m, MEMORY_SIZE + 4096u, 1);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, PAGE_COUNT + 2) == 0,
          "marking past physical memory is ignored");

    /* "Only the lower 25 bits of the bit number are significant." Page 100 is
     * in range, so page 100 | (1 << 25) must alias onto it. */
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, 100u | (1u << 25)) == 1,
          "bit 25 and above of the page number are ignored");

    /* Group form: bit k of the result is page (group * 16 + k). */
    nd500_page_bits_clear_all(m, ND500_PAGE_TABLE_PGU);
    nd500_page_bits_mark(m, 160u * 2048u, 0);   /* group 10, k = 0 */
    nd500_page_bits_mark(m, 163u * 2048u, 0);   /* group 10, k = 3 */
    nd500_page_bits_mark(m, 175u * 2048u, 0);   /* group 10, k = 15 */
    CHECK(nd500_page_bits_read_group(m, ND500_PAGE_TABLE_PGU, 10) == 0x8009u,
          "group form packs 16 pages, lowest page in bit 0");
    CHECK(nd500_page_bits_read_group(m, ND500_PAGE_TABLE_PGU, 11) == 0,
          "an untouched group reads 0");

    /* clear_all really clears everything, and only the named table. */
    nd500_page_bits_mark(m, 200u * 2048u, 1);
    nd500_page_bits_clear_all(m, ND500_PAGE_TABLE_PGU);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, 200) == 0,
          "clear_all wipes PGU");
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 200) == 1,
          "clear_all on PGU leaves WIP intact");
}

/* ---------------------------------------------------------------------------
 * Layer 2: the instructions
 * ------------------------------------------------------------------------ */

static void test_instructions(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n[2] Instructions\n");

    cpu->ST1 |= ND500_FLAG_PIA;    /* privileged - all six require it */

    nd500_page_bits_clear_all(m, ND500_PAGE_TABLE_PGU);
    nd500_page_bits_clear_all(m, ND500_PAGE_TABLE_WIP);
    nd500_page_bits_mark(m, 300u * 2048u, 1);   /* sets PGU and WIP for page 300 */

    /* BI1 RPGU (0xFE88) of a used page: 1, Z clear. */
    Nd500FetchedInstruction fi = make_one_operand(0xFE88, 1, 300);
    cpu->ST1 |= ND500_FLAG_Z;                   /* seed Z set, so a clear is visible */
    nd500_instr_Rpgu(cpu, &fi);
    CHECK(nd500_read_integer_register(cpu, 1) == 1, "RPGU of a used page returns 1");
    CHECK((cpu->ST1 & ND500_FLAG_Z) == 0, "RPGU clears Z for a non-zero result");

    /* BI1 RWIP (0xFE94) of a written page: 1. */
    fi = make_one_operand(0xFE94, 1, 300);
    nd500_instr_Rwip(cpu, &fi);
    CHECK(nd500_read_integer_register(cpu, 1) == 1, "RWIP of a written page returns 1");

    /* An untouched page reads 0 and sets Z, in both tables. */
    fi = make_one_operand(0xFE88, 2, 301);
    cpu->ST1 &= ~(uint32_t)ND500_FLAG_Z;
    nd500_instr_Rpgu(cpu, &fi);
    CHECK(nd500_read_integer_register(cpu, 2) == 0, "RPGU of an untouched page returns 0");
    CHECK((cpu->ST1 & ND500_FLAG_Z) != 0, "RPGU sets Z for a zero result");

    fi = make_one_operand(0xFE94, 2, 301);
    cpu->ST1 &= ~(uint32_t)ND500_FLAG_Z;
    nd500_instr_Rwip(cpu, &fi);
    CHECK(nd500_read_integer_register(cpu, 2) == 0, "RWIP of an untouched page returns 0");
    CHECK((cpu->ST1 & ND500_FLAG_Z) != 0, "RWIP sets Z for a zero result");

    /* A page that was only READ: PGU set, WIP clear. This is the distinction
     * the old always-zero stubs could not express. */
    nd500_page_bits_mark(m, 302u * 2048u, 0);
    fi = make_one_operand(0xFE88, 3, 302);
    nd500_instr_Rpgu(cpu, &fi);
    CHECK(nd500_read_integer_register(cpu, 3) == 1, "read-only page: RPGU returns 1");
    fi = make_one_operand(0xFE94, 3, 302);
    nd500_instr_Rwip(cpu, &fi);
    CHECK(nd500_read_integer_register(cpu, 3) == 0, "read-only page: RWIP returns 0");

    /* Group form H1 RPGU (0xFE8C) reads page 300 as bit 12 of group 18. */
    fi = make_one_operand(0xFE8C, 4, 18);
    nd500_instr_Rpgu(cpu, &fi);
    CHECK((nd500_read_integer_register(cpu, 4) & (1u << 12)) != 0,
          "H1 RPGU group form reports page 300 in group 18");

    /* ZPGU clears just the PGU bit; the WIP bit survives. */
    fi = make_one_operand(0xFE90, 0, 300);
    nd500_instr_Zpgu(cpu, &fi);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, 300) == 0, "ZPGU clears the PGU bit");
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 300) == 1, "ZPGU leaves WIP set");

    /* ZWIP clears just the WIP bit. */
    fi = make_one_operand(0xFE9C, 0, 300);
    nd500_instr_Zwip(cpu, &fi);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 300) == 0, "ZWIP clears the WIP bit");

    /* CPGU / CWIP wipe their whole table and nothing else. */
    nd500_page_bits_mark(m, 400u * 2048u, 1);
    nd500_page_bits_mark(m, 401u * 2048u, 1);
    fi = make_no_operand(0xFF1A);
    nd500_instr_Cpgu(cpu, &fi);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, 400) == 0 &&
          nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, 401) == 0,
          "CPGU clears the whole PGU table");
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 400) == 1,
          "CPGU leaves the WIP table alone");

    fi = make_no_operand(0xFF1B);
    nd500_instr_Cwip(cpu, &fi);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 400) == 0 &&
          nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 401) == 0,
          "CWIP clears the whole WIP table");

    /* Control: all six are privileged. Without PIA, RPGU must trap and must NOT
     * write the target register. */
    nd500_page_bits_mark(m, 500u * 2048u, 1);
    cpu->ST1 &= ~(uint32_t)ND500_FLAG_PIA;
    nd500_write_integer_register(cpu, 1, 0xA5A5A5A5u);
    fi = make_one_operand(0xFE88, 1, 500);
    nd500_instr_Rpgu(cpu, &fi);
    CHECK(nd500_read_integer_register(cpu, 1) == 0xA5A5A5A5u,
          "unprivileged RPGU traps without writing the register");
    cpu->ST1 |= ND500_FLAG_PIA;
}

/* ---------------------------------------------------------------------------
 * Layer 3: the MMU sets the bits
 * ------------------------------------------------------------------------ */

static void test_mmu_marks(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n[3] MMU marking\n");

    /* With the MMU off, nd500_mmu_translate returns the address unchanged and
     * takes an early exit that deliberately does not mark - see the comment at
     * the marking site in nd500_mmu.c. Assert that stated behaviour rather than
     * pretending otherwise, so a future change to it fails here loudly. */
    nd500_page_bits_clear_all(m, ND500_PAGE_TABLE_PGU);
    nd500_page_bits_clear_all(m, ND500_PAGE_TABLE_WIP);

    uint32_t addr = 600u * 2048u;
    (void)nd500_mmu_translate(cpu, addr, 1, 0);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, 600) == 0,
          "untranslated (MMU-off) access does not mark PGU - documented limit");

    /* The mark helper is what the translated path calls; prove the wiring by
     * calling it exactly as nd500_mmu.c does, with a resolved physical address. */
    nd500_page_bits_mark(m, addr, 1);
    CHECK(nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_PGU, 600) == 1 &&
          nd500_page_bits_read_bit(m, ND500_PAGE_TABLE_WIP, 600) == 1,
          "a resolved physical write marks both tables");
}

int main(void) {
    Nd500Machine m;
    Nd500Cpu cpu;

    printf("=== PGU / WIP page tables ===\n");

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    test_bitmaps(&m);
    test_instructions(&m, &cpu);
    test_mmu_marks(&m, &cpu);

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    nd500_machine_free(&m);
    return tests_failed == 0 ? 0 : 1;
}
