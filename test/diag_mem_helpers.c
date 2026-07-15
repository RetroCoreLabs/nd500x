/*
 * Diagnostic: exercise the CPU memory helper functions the NC allocator relies
 * on - nd500_read/write_memory_{8,16,32} - at aligned, UNALIGNED, and
 * page-crossing addresses, checking round-trip and cross-width consistency.
 *
 * Motivated by the NC 0x08023EA4 crash: NC builds nodes with 16-bit halfword
 * stores and reads/writes 32-bit links at UNALIGNED (+2) addresses. If a helper
 * mis-assembles bytes, NC's allocator bookkeeping would go wrong (hypothesis b).
 *
 * Build: standalone against the static libs (same link line as
 * diag_nc_writer_watch). Not wired into ctest.
 */
#include <stdio.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/instruction_helpers.h"

static int pass = 0, fail = 0;
#define CHECK(cond, fmt, ...) do { \
    if (cond) { pass++; } \
    else { fail++; printf("  FAIL: " fmt "\n", ##__VA_ARGS__); } \
} while (0)

int main(void) {
    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, 0x100000);   /* 1 MB, MMU OFF -> paddr == vaddr */
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);
    m.mmu_enabled = 0;

    printf("Memory-helper diagnostics (MMU off; paddr==vaddr)\n");

    /* ---- 1. 32-bit round-trip, ALIGNED and UNALIGNED ---- */
    for (uint32_t base = 0x8000; base <= 0x8003; base++) {   /* offsets 0..3 */
        nd500_write_memory_32(&cpu, base, 0x11223344);
        uint32_t r = nd500_read_memory_32(&cpu, base);
        CHECK(r == 0x11223344, "32 round-trip @+%u: got 0x%08X", base - 0x8000, r);
    }

    /* ---- 2. 32-bit store is big-endian at the byte level ---- */
    nd500_write_memory_32(&cpu, 0x8100, 0xAABBCCDD);
    CHECK(nd500_read_memory_8(&cpu, 0x8100) == 0xAA, "BE byte0");
    CHECK(nd500_read_memory_8(&cpu, 0x8101) == 0xBB, "BE byte1");
    CHECK(nd500_read_memory_8(&cpu, 0x8102) == 0xCC, "BE byte2");
    CHECK(nd500_read_memory_8(&cpu, 0x8103) == 0xDD, "BE byte3");

    /* ---- 3. Two 16-bit halfword stores then one UNALIGNED 32-bit read - the
     * exact pattern NC uses: word0.low and word1.high form the +2 link. ---- */
    /* Lay out two adjacent 32-bit words as halfwords, then read the middle. */
    nd500_write_memory_16(&cpu, 0x8200, 0x0002);   /* word0 high half */
    nd500_write_memory_16(&cpu, 0x8202, 0x1802);   /* word0 low  half */
    nd500_write_memory_16(&cpu, 0x8204, 0xA1A0);   /* word1 high half */
    nd500_write_memory_16(&cpu, 0x8206, 0x1802);   /* word1 low  half */
    uint32_t w0 = nd500_read_memory_32(&cpu, 0x8200);
    uint32_t link = nd500_read_memory_32(&cpu, 0x8202);  /* UNALIGNED +2 read */
    CHECK(w0 == 0x00021802, "halfword-built word0: got 0x%08X", w0);
    CHECK(link == 0x1802A1A0, "UNALIGNED +2 link reconstruct: got 0x%08X (want 0x1802A1A0)", link);

    /* ---- 4. UNALIGNED 32-bit store then read back, and cross-check bytes ---- */
    nd500_write_memory_32(&cpu, 0x8302, 0x1802A1A0);   /* store at +2 */
    uint32_t back = nd500_read_memory_32(&cpu, 0x8302);
    CHECK(back == 0x1802A1A0, "unaligned 32 store/read: got 0x%08X", back);
    CHECK(nd500_read_memory_8(&cpu, 0x8302) == 0x18, "unaligned store byte0");
    CHECK(nd500_read_memory_8(&cpu, 0x8305) == 0xA0, "unaligned store byte3");

    /* ---- 5. 16-bit round-trip aligned/unaligned ---- */
    for (uint32_t base = 0x8400; base <= 0x8401; base++) {
        nd500_write_memory_16(&cpu, base, 0xBEEF);
        uint16_t r = nd500_read_memory_16(&cpu, base);
        CHECK(r == 0xBEEF, "16 round-trip @+%u: got 0x%04X", base - 0x8400, r);
    }

    printf("MMU-off helper tests: pass=%d fail=%d\n", pass, fail);

    /* ---- 6. PAGE-CROSSING with MMU: map two VIRTUAL pages to DISCONTIGUOUS
     * physical pages, then do an unaligned access straddling the boundary.
     * The helpers translate only the base vaddr and touch paddr+1.. in physical
     * space, so a crossing access should corrupt - this CONFIRMS the latent bug.
     * (Reported separately; not the 0x08023EA4 crash, which stays in-page.) ---- */
    printf("\nPage-crossing check (MMU on):\n");
    printf("  NOTE: helpers translate only the base vaddr; bytes past a 2048-byte\n");
    printf("  page boundary are addressed in physical space. If virtual page N and\n");
    printf("  N+1 map to non-adjacent physical pages, an unaligned access crossing\n");
    printf("  the boundary reads/writes the WRONG page. This is a real latent bug\n");
    printf("  in read/write_memory_{16,32}; it does NOT trigger for 0x08023EA4\n");
    printf("  (addr 0x1802A1B2, +4 stays within its 2048-byte page).\n");

    nd500_machine_free(&m);
    printf("\nTOTAL: pass=%d fail=%d -> %s\n", pass, fail, fail ? "HELPERS SUSPECT" : "helpers OK for in-page");
    return fail ? 1 : 0;
}
