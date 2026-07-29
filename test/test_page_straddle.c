/**
 * ND500 Page-Straddling Memory Access Test
 *
 * A 16- or 32-bit access used to be translated through the MMU ONCE, for its
 * FIRST byte, after which the remaining bytes were read/written at
 * paddr+1 .. paddr+N-1. That is only correct while the whole access stays
 * inside one page. ND-500 pages are NBPG = 2048 bytes and consecutive VIRTUAL
 * pages are NOT physically contiguous, so an access crossing a 2KB boundary
 * put its tail bytes into whatever frame physically FOLLOWED the first frame
 * instead of the frame the next virtual page maps to.
 *
 * The real failure (NDIX multiuser boot): a 32-bit store to the u-area kernel
 * stack at virtual 0xE8000FFE put its last two bytes into physical
 * 0x004AB000/0x004AB001 - page 0 of ANOTHER process' u-area - changing that
 * process' u_pcb from 0xE0000300 to 0xD5640300. The kernel then dereferenced
 * the wild pointer and died at PC=0x00037B16.
 *
 * The accessors now detect ((vaddr & (NBPG-1)) + span) > NBPG and fall back to
 * per-byte accesses, each translated on its own, in all eight 16/32-bit
 * accessors (read/write x 16/32-bit x plain/_domain). The 64-bit accessors
 * delegate to two 32-bit calls and inherit the fix.
 *
 * Test mapping (PS_ASI, single-level page table), deliberately NON-ADJACENT
 * physical frames so a straddle that is not split is immediately visible:
 *
 *   virtual page 0  0x18000000  -> PFN 0x200 (physical 0x100000)
 *   virtual page 1  0x18000800  -> PFN 0x300 (physical 0x180000)
 *   virtual page 2  0x18001000  -> PFN 0x400 (physical 0x200000)
 *
 * The frames that physically FOLLOW the mapped ones (PFN 0x201 = 0x100800 and
 * PFN 0x301 = 0x180800) are mapped by NO virtual page and are pre-filled with
 * a 0xCC guard pattern: any byte of them that changes is the old bug.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/cpu/nd500_mmu.h"

#define MEMORY_SIZE     (8 * 1024 * 1024)   /* 8MB */

#define TEST_SEGMENT    3                   /* virtual base = segment << 27 */
#define SEG_BASE        (((uint32_t)TEST_SEGMENT) << SGSHIFT)
#define TEST_PSN        100                 /* Physical Segment Number */
#define TEST_DOMAIN     0                   /* used by the plain accessors (CED) */
#define ALT_DOMAIN      7                   /* used by the _domain accessors */

#define PT_PFN          0x100u              /* page table lives at 0x080000 */
#define PFN_PAGE0       0x200u              /* 0x100000 */
#define PFN_GUARD0      0x201u              /* 0x100800 - physically follows page 0 */
#define PFN_PAGE1       0x300u              /* 0x180000 */
#define PFN_GUARD1      0x301u              /* 0x180800 - physically follows page 1 */
#define PFN_PAGE2       0x400u              /* 0x200000 */

#define PHYS(pfn)       (((uint32_t)(pfn)) << PGSHIFT)

#define VA_PAGE0        (SEG_BASE + 0u)
#define VA_PAGE1        (SEG_BASE + 2048u)
#define VA_PAGE2        (SEG_BASE + 4096u)

#define GUARD_BYTE      0xCC

static int tests_passed = 0;
static int tests_failed = 0;

static void test_result(const char* name, bool passed, const char* details) {
    if (passed) {
        tests_passed++;
        printf("  [PASS] %s\n", name);
    } else {
        tests_failed++;
        printf("  [FAIL] %s: %s\n", name, details);
    }
}

/* Raw physical accessors - bypass the MMU entirely */
static uint8_t phys_read8(Nd500Machine* m, uint32_t paddr) {
    return nd500_bus_read8(m, paddr);
}

static void phys_fill(Nd500Machine* m, uint32_t paddr, uint8_t value, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) nd500_bus_write8(m, paddr + i, value);
}

/* Install one PS_ASI page table entry: virtual page index -> physical frame */
static void map_page(Nd500Cpu* cpu, uint32_t l2_index, uint32_t pfn) {
    PageTableEntry pte;
    pte.valid = 1;
    pte.protection = PG_W;          /* 0 = writable */
    pte.physical_pfn = pfn;
    nd500_mmu_write_pte(cpu, PHYS(PT_PFN) + l2_index * 4u, pte);
}

static void setup_mmu(Nd500Machine* m, Nd500Cpu* cpu) {
    nd500_mmu_init(cpu);

    /* Segment TEST_SEGMENT of both domains -> PSN TEST_PSN, writable */
    nd500_mmu_set_data_capability(cpu, TEST_DOMAIN, TEST_SEGMENT, TEST_PSN | DC_WRP);
    nd500_mmu_set_data_capability(cpu, ALT_DOMAIN,  TEST_SEGMENT, TEST_PSN | DC_WRP);

    /* PSN TEST_PSN is a single-level (PS_ASI) paged segment */
    nd500_mmu_set_pst_entry(cpu, TEST_PSN, PS_ASI, PT_PFN);

    phys_fill(m, PHYS(PT_PFN), 0x00, NBPG);     /* all pages absent by default */
    map_page(cpu, 0, PFN_PAGE0);
    map_page(cpu, 1, PFN_PAGE1);
    map_page(cpu, 2, PFN_PAGE2);

    cpu->CED = TEST_DOMAIN;
    cpu->CAD = ALT_DOMAIN;

    nd500_mmu_enable_data(cpu);
}

/* Zero the mapped frames, refill the unmapped guard frames with 0xCC */
static void reset_frames(Nd500Machine* m) {
    phys_fill(m, PHYS(PFN_PAGE0), 0x00, NBPG);
    phys_fill(m, PHYS(PFN_PAGE1), 0x00, NBPG);
    phys_fill(m, PHYS(PFN_PAGE2), 0x00, NBPG);
    phys_fill(m, PHYS(PFN_GUARD0), GUARD_BYTE, NBPG);
    phys_fill(m, PHYS(PFN_GUARD1), GUARD_BYTE, NBPG);
    nd500_trap_clear();
}

static bool guard_intact(Nd500Machine* m, uint32_t pfn) {
    for (uint32_t i = 0; i < NBPG; i++) {
        if (phys_read8(m, PHYS(pfn) + i) != GUARD_BYTE) return false;
    }
    return true;
}

/* ------------------------------------------------------------------------
 * (a) VALIDATION - the ordinary, non-straddling path is unchanged
 * ------------------------------------------------------------------------ */
static void test_non_straddling(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n=== Non-straddling accesses (unchanged behaviour) ===\n");

    /* 32-bit well inside page 0 */
    {
        reset_frames(m);
        uint32_t va = VA_PAGE0 + 0x100;
        nd500_write_memory_32(cpu, va, 0x11223344u);
        uint32_t back = nd500_read_memory_32(cpu, va);
        uint32_t pa = PHYS(PFN_PAGE0) + 0x100;
        bool passed = (back == 0x11223344u)
                   && phys_read8(m, pa + 0) == 0x11 && phys_read8(m, pa + 1) == 0x22
                   && phys_read8(m, pa + 2) == 0x33 && phys_read8(m, pa + 3) == 0x44;
        char d[160];
        snprintf(d, sizeof(d), "read back 0x%08X, phys %02X %02X %02X %02X", back,
                 phys_read8(m, pa), phys_read8(m, pa+1), phys_read8(m, pa+2), phys_read8(m, pa+3));
        test_result("32-bit inside one page (offset 0x100)", passed, d);
    }

    /* 16-bit well inside page 1 */
    {
        reset_frames(m);
        uint32_t va = VA_PAGE1 + 0x200;
        nd500_write_memory_16(cpu, va, 0xABCDu);
        uint16_t back = nd500_read_memory_16(cpu, va);
        uint32_t pa = PHYS(PFN_PAGE1) + 0x200;
        bool passed = (back == 0xABCDu)
                   && phys_read8(m, pa + 0) == 0xAB && phys_read8(m, pa + 1) == 0xCD;
        char d[160];
        snprintf(d, sizeof(d), "read back 0x%04X, phys %02X %02X", back,
                 phys_read8(m, pa), phys_read8(m, pa+1));
        test_result("16-bit inside one page (offset 0x200)", passed, d);
    }

    /* 32-bit ending EXACTLY on the page boundary is NOT a straddle */
    {
        reset_frames(m);
        uint32_t va = VA_PAGE0 + 0x7FC;
        nd500_write_memory_32(cpu, va, 0xDEADBEEFu);
        uint32_t back = nd500_read_memory_32(cpu, va);
        uint32_t pa = PHYS(PFN_PAGE0) + 0x7FC;
        bool passed = (back == 0xDEADBEEFu)
                   && phys_read8(m, pa + 0) == 0xDE && phys_read8(m, pa + 1) == 0xAD
                   && phys_read8(m, pa + 2) == 0xBE && phys_read8(m, pa + 3) == 0xEF
                   && guard_intact(m, PFN_GUARD0);
        char d[160];
        snprintf(d, sizeof(d), "read back 0x%08X, guard %s", back,
                 guard_intact(m, PFN_GUARD0) ? "intact" : "TOUCHED");
        test_result("32-bit ending exactly at page end (offset 0x7FC)", passed, d);
    }
}

/* ------------------------------------------------------------------------
 * (a) VALIDATION - every straddling alignment lands in the right frame
 * ------------------------------------------------------------------------ */
static void test_straddle_write_read_32(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n=== 32-bit straddling writes/reads (all alignments) ===\n");

    /* offset -> how many of the four bytes stay in page 0 */
    static const uint32_t offsets[3] = { 0x7FD, 0x7FE, 0x7FF };
    static const uint32_t in_page0[3] = { 3, 2, 1 };

    for (int t = 0; t < 3; t++) {
        reset_frames(m);
        uint32_t off = offsets[t];
        uint32_t va = VA_PAGE0 + off;
        uint32_t value = 0xA1B2C3D4u;
        uint8_t expect[4] = { 0xA1, 0xB2, 0xC3, 0xD4 };

        nd500_write_memory_32(cpu, va, value);

        bool passed = true;
        for (uint32_t i = 0; i < 4; i++) {
            uint32_t pa = (i < in_page0[t])
                ? PHYS(PFN_PAGE0) + off + i
                : PHYS(PFN_PAGE1) + (off + i - NBPG);
            if (phys_read8(m, pa) != expect[i]) passed = false;
        }
        if (!guard_intact(m, PFN_GUARD0)) passed = false;

        uint32_t back = nd500_read_memory_32(cpu, va);
        if (back != value) passed = false;

        char name[96], d[192];
        snprintf(name, sizeof(name), "write32+read32 straddle at offset 0x%03X (%u|%u bytes)",
                 off, in_page0[t], 4 - in_page0[t]);
        snprintf(d, sizeof(d), "read back 0x%08X, page0 tail %02X %02X %02X, page1 head %02X %02X %02X, guard %s",
                 back,
                 phys_read8(m, PHYS(PFN_PAGE0) + 0x7FD), phys_read8(m, PHYS(PFN_PAGE0) + 0x7FE),
                 phys_read8(m, PHYS(PFN_PAGE0) + 0x7FF),
                 phys_read8(m, PHYS(PFN_PAGE1) + 0), phys_read8(m, PHYS(PFN_PAGE1) + 1),
                 phys_read8(m, PHYS(PFN_PAGE1) + 2),
                 guard_intact(m, PFN_GUARD0) ? "intact" : "TOUCHED");
        test_result(name, passed, d);
    }

    /* A straddling READ must gather from both frames - seed the frames directly */
    {
        reset_frames(m);
        phys_fill(m, PHYS(PFN_PAGE0) + 0x7FE, 0x00, 2);
        nd500_bus_write8(m, PHYS(PFN_PAGE0) + 0x7FE, 0x12);
        nd500_bus_write8(m, PHYS(PFN_PAGE0) + 0x7FF, 0x34);
        nd500_bus_write8(m, PHYS(PFN_PAGE1) + 0x000, 0x56);
        nd500_bus_write8(m, PHYS(PFN_PAGE1) + 0x001, 0x78);
        /* The frame that physically follows page 0 holds a decoy: the old code
         * would have read 0xCCCC as the tail. */
        uint32_t back = nd500_read_memory_32(cpu, VA_PAGE0 + 0x7FE);
        char d[96];
        snprintf(d, sizeof(d), "got 0x%08X, expected 0x12345678", back);
        test_result("read32 straddle gathers from both frames (not the following frame)",
                    back == 0x12345678u, d);
    }
}

static void test_straddle_write_read_16(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n=== 16-bit straddling writes/reads ===\n");

    {
        reset_frames(m);
        uint32_t va = VA_PAGE0 + 0x7FF;         /* 1 byte in each page */
        nd500_write_memory_16(cpu, va, 0x9A5Cu);

        bool passed = phys_read8(m, PHYS(PFN_PAGE0) + 0x7FF) == 0x9A
                   && phys_read8(m, PHYS(PFN_PAGE1) + 0x000) == 0x5C
                   && guard_intact(m, PFN_GUARD0);
        uint16_t back = nd500_read_memory_16(cpu, va);
        if (back != 0x9A5Cu) passed = false;

        char d[160];
        snprintf(d, sizeof(d), "read back 0x%04X, page0[0x7FF]=%02X page1[0]=%02X, guard %s",
                 back, phys_read8(m, PHYS(PFN_PAGE0) + 0x7FF),
                 phys_read8(m, PHYS(PFN_PAGE1) + 0), guard_intact(m, PFN_GUARD0) ? "intact" : "TOUCHED");
        test_result("write16+read16 straddle at offset 0x7FF", passed, d);
    }

    /* Same across the page1/page2 boundary, to prove it is not specific to
     * one particular pair of frames. */
    {
        reset_frames(m);
        uint32_t va = VA_PAGE1 + 0x7FF;
        nd500_write_memory_16(cpu, va, 0x0FF0u);

        bool passed = phys_read8(m, PHYS(PFN_PAGE1) + 0x7FF) == 0x0F
                   && phys_read8(m, PHYS(PFN_PAGE2) + 0x000) == 0xF0
                   && guard_intact(m, PFN_GUARD1);
        uint16_t back = nd500_read_memory_16(cpu, va);
        if (back != 0x0FF0u) passed = false;

        char d[160];
        snprintf(d, sizeof(d), "read back 0x%04X, page1[0x7FF]=%02X page2[0]=%02X, guard %s",
                 back, phys_read8(m, PHYS(PFN_PAGE1) + 0x7FF),
                 phys_read8(m, PHYS(PFN_PAGE2) + 0), guard_intact(m, PFN_GUARD1) ? "intact" : "TOUCHED");
        test_result("write16+read16 straddle across the page1/page2 boundary", passed, d);
    }
}

/* ------------------------------------------------------------------------
 * (a) VALIDATION - the _domain accessor variants carry the same fix
 * ------------------------------------------------------------------------ */
static void test_straddle_domain_variants(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n=== _domain accessor variants (domain %d) ===\n", ALT_DOMAIN);

    /* 32-bit, 2|2 split */
    {
        reset_frames(m);
        uint32_t va = VA_PAGE0 + 0x7FE;
        nd500_write_memory_32_domain(cpu, va, 0x01020304u, ALT_DOMAIN);

        bool passed = phys_read8(m, PHYS(PFN_PAGE0) + 0x7FE) == 0x01
                   && phys_read8(m, PHYS(PFN_PAGE0) + 0x7FF) == 0x02
                   && phys_read8(m, PHYS(PFN_PAGE1) + 0x000) == 0x03
                   && phys_read8(m, PHYS(PFN_PAGE1) + 0x001) == 0x04
                   && guard_intact(m, PFN_GUARD0);
        uint32_t back = nd500_read_memory_32_domain(cpu, va, ALT_DOMAIN);
        if (back != 0x01020304u) passed = false;

        char d[160];
        snprintf(d, sizeof(d), "read back 0x%08X, guard %s", back,
                 guard_intact(m, PFN_GUARD0) ? "intact" : "TOUCHED");
        test_result("write32_domain+read32_domain straddle at offset 0x7FE", passed, d);
    }

    /* 16-bit, 1|1 split */
    {
        reset_frames(m);
        uint32_t va = VA_PAGE0 + 0x7FF;
        nd500_write_memory_16_domain(cpu, va, 0xBEEFu, ALT_DOMAIN);

        bool passed = phys_read8(m, PHYS(PFN_PAGE0) + 0x7FF) == 0xBE
                   && phys_read8(m, PHYS(PFN_PAGE1) + 0x000) == 0xEF
                   && guard_intact(m, PFN_GUARD0);
        uint16_t back = nd500_read_memory_16_domain(cpu, va, ALT_DOMAIN);
        if (back != 0xBEEFu) passed = false;

        char d[160];
        snprintf(d, sizeof(d), "read back 0x%04X, guard %s", back,
                 guard_intact(m, PFN_GUARD0) ? "intact" : "TOUCHED");
        test_result("write16_domain+read16_domain straddle at offset 0x7FF", passed, d);
    }
}

/* ------------------------------------------------------------------------
 * (a) VALIDATION - a 64-bit access that straddles (delegates to two 32-bit)
 * ------------------------------------------------------------------------ */
static void test_straddle_64(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n=== 64-bit straddling access ===\n");

    {
        reset_frames(m);
        uint32_t va = VA_PAGE0 + 0x7FE;         /* 2 bytes in page 0, 6 in page 1 */
        uint64_t value = 0x0102030405060708ULL;
        uint8_t expect[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

        nd500_write_memory_64(cpu, va, value);

        bool passed = true;
        for (uint32_t i = 0; i < 8; i++) {
            uint32_t pa = (i < 2) ? PHYS(PFN_PAGE0) + 0x7FE + i
                                  : PHYS(PFN_PAGE1) + (i - 2);
            if (phys_read8(m, pa) != expect[i]) passed = false;
        }
        if (!guard_intact(m, PFN_GUARD0)) passed = false;

        uint64_t back = nd500_read_memory_64(cpu, va);
        if (back != value) passed = false;

        char d[160];
        snprintf(d, sizeof(d), "read back 0x%016llX, guard %s",
                 (unsigned long long)back, guard_intact(m, PFN_GUARD0) ? "intact" : "TOUCHED");
        test_result("write64+read64 straddle at offset 0x7FE (2|6 bytes)", passed, d);
    }

    /* 64-bit _domain variant */
    {
        reset_frames(m);
        uint32_t va = VA_PAGE0 + 0x7FE;
        uint64_t value = 0xFEDCBA9876543210ULL;
        nd500_write_memory_64_domain(cpu, va, value, ALT_DOMAIN);
        uint64_t back = nd500_read_memory_64_domain(cpu, va, ALT_DOMAIN);

        bool passed = (back == value)
                   && phys_read8(m, PHYS(PFN_PAGE0) + 0x7FE) == 0xFE
                   && phys_read8(m, PHYS(PFN_PAGE0) + 0x7FF) == 0xDC
                   && phys_read8(m, PHYS(PFN_PAGE1) + 0x000) == 0xBA
                   && guard_intact(m, PFN_GUARD0);
        char d[160];
        snprintf(d, sizeof(d), "read back 0x%016llX, guard %s",
                 (unsigned long long)back, guard_intact(m, PFN_GUARD0) ? "intact" : "TOUCHED");
        test_result("write64_domain+read64_domain straddle at offset 0x7FE", passed, d);
    }
}

/* ------------------------------------------------------------------------
 * (b) REGRESSION - reproduces the NDIX u-area corruption shape
 *
 * BEFORE THE FIX this test fails: the two tail bytes of the 32-bit store land
 * in the frame that physically FOLLOWS page 0 (the guard frame), leaving
 * page 1 - the frame the next virtual page actually maps to - untouched.
 * That is exactly how a store to u-area VA 0xE8000FFE clobbered another
 * process' u_pcb (0xE0000300 -> 0xD5640300) and killed the kernel at
 * PC=0x00037B16.
 * ------------------------------------------------------------------------ */
static void test_regression_ndix_uarea_shape(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n=== REGRESSION: NDIX u-area straddling store (bug 8e063db) ===\n");

    reset_frames(m);

    /* The same store shape as ino_close()'s
     *   cfunc = cdevsw[major(dev)].d_close;
     * to the kernel stack at u-area VA 0xE8000FFE - offset 0x7FE of a page. */
    uint32_t va = VA_PAGE0 + 0x7FE;
    uint32_t value = 0xE0000300u;               /* the u_pcb value that got shredded */

    nd500_write_memory_32(cpu, va, value);

    bool tail_in_page1 = phys_read8(m, PHYS(PFN_PAGE1) + 0x000) == 0x03
                      && phys_read8(m, PHYS(PFN_PAGE1) + 0x001) == 0x00;
    bool head_in_page0 = phys_read8(m, PHYS(PFN_PAGE0) + 0x7FE) == 0xE0
                      && phys_read8(m, PHYS(PFN_PAGE0) + 0x7FF) == 0x00;
    bool following_frame_untouched = guard_intact(m, PFN_GUARD0);
    uint32_t back = nd500_read_memory_32(cpu, va);

    {
        char d[256];
        snprintf(d, sizeof(d),
                 "following frame 0x%08X now reads %02X %02X (must stay 0xCC 0xCC) - "
                 "this is the byte pattern that overwrote another process' u_pcb",
                 PHYS(PFN_GUARD0), phys_read8(m, PHYS(PFN_GUARD0)),
                 phys_read8(m, PHYS(PFN_GUARD0) + 1));
        test_result("tail bytes did NOT leak into the physically following frame",
                    following_frame_untouched, d);
    }
    {
        char d[192];
        snprintf(d, sizeof(d), "page1[0]=%02X page1[1]=%02X, expected 03 00",
                 phys_read8(m, PHYS(PFN_PAGE1) + 0), phys_read8(m, PHYS(PFN_PAGE1) + 1));
        test_result("tail bytes landed in the frame virtual page 1 maps to",
                    tail_in_page1, d);
    }
    {
        char d[192];
        snprintf(d, sizeof(d), "page0[0x7FE]=%02X page0[0x7FF]=%02X, expected E0 00",
                 phys_read8(m, PHYS(PFN_PAGE0) + 0x7FE), phys_read8(m, PHYS(PFN_PAGE0) + 0x7FF));
        test_result("head bytes stayed in the frame virtual page 0 maps to",
                    head_in_page0, d);
    }
    {
        char d[96];
        snprintf(d, sizeof(d), "read back 0x%08X, expected 0x%08X", back, value);
        test_result("straddling store reads back intact (u_pcb not shredded)",
                    back == value, d);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("ND500 Page-Straddling Memory Access Tests\n");
    printf("=========================================\n");
    printf("NBPG = %d bytes per page\n", NBPG);

    nd500_quiet = 1;

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    setup_mmu(&m, &cpu);

    /* Sanity: the mapping really is non-adjacent, otherwise nothing below proves
     * anything. */
    {
        uint32_t p0 = nd500_mmu_translate(&cpu, VA_PAGE0, 0, 0);
        uint32_t p1 = nd500_mmu_translate(&cpu, VA_PAGE1, 0, 0);
        char d[160];
        snprintf(d, sizeof(d), "page0 -> 0x%08X, page1 -> 0x%08X (must not be 2048 apart)", p0, p1);
        test_result("test mapping: consecutive virtual pages are NOT physically adjacent",
                    p0 == PHYS(PFN_PAGE0) && p1 == PHYS(PFN_PAGE1) && (p1 != p0 + NBPG), d);
    }

    test_non_straddling(&m, &cpu);
    test_straddle_write_read_32(&m, &cpu);
    test_straddle_write_read_16(&m, &cpu);
    test_straddle_domain_variants(&m, &cpu);
    test_straddle_64(&m, &cpu);
    test_regression_ndix_uarea_shape(&m, &cpu);

    nd500_mmu_disable(&cpu);
    nd500_machine_free(&m);

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
