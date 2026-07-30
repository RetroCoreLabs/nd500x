/*
 * Physical page allocator + concurrent-domain residency test
 *
 * Two things are checked here, both of which were broken before the machine
 * owned its physical pages:
 *
 *  1. The allocator itself: pages are never handed out twice, a freed page
 *     comes back, reserved pages are never handed out, and closing an arena
 *     frees exactly what was allocated inside it - not what an outer scope
 *     (or a permanent reservation) holds.
 *
 *  2. Two DOMs resident at the same time. Every domain used to be loaded at
 *     the same fixed physical base, so loading a second one wrote straight
 *     over the first one's code and data. The test loads DOM A, records its
 *     resident image, loads DOM B into a nested scope, and requires A's bytes
 *     and page-table entries to be untouched - which is what lets a domain
 *     start another domain (MON 317B UECOM, or a new command) and resume.
 *
 * Usage: test_phys_alloc <dom_a> <dom_b>
 * Exit:  0 = all checks passed, 1 = a check failed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_phys_alloc.h"
#include "../src/machine/machine_protos.h"
#include "../src/ndlib/ndlib.h"

#define MEMORY_SIZE (16 * 1024 * 1024)

static int g_failures = 0;

static void check(int cond, const char* what) {
    printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) g_failures++;
}

/* ---------------------------------------------------------------- part 1 */

static void test_allocator(Nd500Machine* m) {
    printf("\nAllocator:\n");

    uint32_t total = nd500_phys_pages_total(m);
    check(total == MEMORY_SIZE / 2048, "page count matches memory size");

    uint32_t free_at_start = nd500_phys_pages_free(m);

    uint32_t a = nd500_phys_alloc_pages(m, 4, /*zero=*/1);
    uint32_t b = nd500_phys_alloc_pages(m, 4, /*zero=*/1);
    check(a != 0 && b != 0, "two allocations succeed");
    check(a + 4 <= b || b + 4 <= a, "allocations do not overlap");
    check(nd500_phys_pages_free(m) == free_at_start - 8, "8 pages accounted for");

    /* Zeroing must actually clear the frame: dirty it, free it, take it back. */
    nd500_bus_write32(m, a << 11, 0xDEADBEEF);
    nd500_phys_free_pages(m, a, 4);
    uint32_t again = nd500_phys_alloc_pages(m, 4, /*zero=*/1);
    check(again == a, "a freed run is reused");
    check(nd500_bus_read8(m, a << 11) == 0, "a recycled page is zeroed");

    nd500_phys_free_pages(m, again, 4);
    nd500_phys_free_pages(m, b, 4);
    check(nd500_phys_pages_free(m) == free_at_start, "everything came back");

    /* Reserved pages are permanent and off-limits. */
    uint32_t r = nd500_phys_alloc_pages(m, 1, 0);
    nd500_phys_free_pages(m, r, 1);
    check(nd500_phys_reserve(m, r << 11, 2048) == 0, "reserve succeeds on a free page");
    check(nd500_phys_page_owner(m, r) == ND500_PHYS_PERM, "reserved page is permanent");

    /* Arenas: an inner scope frees only its own pages. */
    uint32_t outer_arena = nd500_phys_arena_push(m);
    uint32_t outer_page = nd500_phys_alloc_pages(m, 2, 1);
    check(outer_page != 0, "outer scope allocates");

    uint32_t inner_arena = nd500_phys_arena_push(m);
    check(inner_arena > outer_arena, "inner arena nests above outer");
    uint32_t inner_page = nd500_phys_alloc_pages(m, 2, 1);
    check(inner_page != 0 && inner_page != outer_page, "inner scope gets other pages");

    nd500_phys_arena_pop(m, inner_arena);
    check(nd500_phys_page_owner(m, inner_page) == ND500_PHYS_FREE, "pop frees inner pages");
    check(nd500_phys_page_owner(m, outer_page) == outer_arena, "pop leaves outer pages held");
    check(nd500_phys_page_owner(m, r) == ND500_PHYS_PERM, "pop leaves reserved pages held");

    nd500_phys_arena_pop(m, outer_arena);
    check(nd500_phys_page_owner(m, outer_page) == ND500_PHYS_FREE, "outer pop frees outer pages");
    check(nd500_phys_page_owner(m, r) == ND500_PHYS_PERM, "reserved page survives every pop");

    /* Popping an already-closed arena must not free a later scope's pages. */
    uint32_t reuse_arena = nd500_phys_arena_push(m);
    uint32_t reuse_page = nd500_phys_alloc_pages(m, 1, 1);
    nd500_phys_arena_pop(m, outer_arena);   /* stale id, already popped */
    check(nd500_phys_page_owner(m, reuse_page) == reuse_arena,
          "a stale pop does not free a live scope");
    nd500_phys_arena_pop(m, reuse_arena);
}

/* ---------------------------------------------------------------- part 2 */

/* Sum the bytes of every page a domain's PROG segments map, so a later change
 * anywhere in its resident image shows up. */
static unsigned long prog_image_digest(Nd500Cpu* cpu, uint8_t domain, int* pages_seen) {
    unsigned long digest = 0;
    *pages_seen = 0;

    for (int seg = 0; seg < 32; seg++) {
        uint16_t pc = nd500_mmu_get_program_capability(cpu, domain, seg);
        if (pc == 0 || (pc & PC_IND)) continue;

        PhysicalSegmentTableEntry e = nd500_mmu_get_pst_entry(cpu, pc & PC_PSN);
        if (e.index_mode != PS_ASI || e.physical_pfn == 0) continue;

        uint32_t table = e.physical_pfn << 11;
        for (uint32_t i = 0; i < 512; i++) {
            uint32_t pte = nd500_bus_read32(cpu->machine, table + i * 4);
            uint32_t pfn = pte & 0x3FFFFFFFu;
            if (pfn == 0) continue;
            uint32_t base = pfn << 11;
            for (uint32_t o = 0; o < 2048; o += 4) {
                digest = digest * 31u + nd500_bus_read32(cpu->machine, base + o);
            }
            (*pages_seen)++;
        }
    }
    return digest;
}

static int load_dom(Nd500Machine* m, Nd500Cpu* cpu, const char* path, int* domain_out) {
    if (ndlib_load_dom_header(path) != 0) return -1;
    if (ndlib_load_dom_segments() != 0) return -1;
    uint32_t start = 0;
    return ndlib_dom_load_to_machine(m, cpu, -1, NULL, NULL, &start, domain_out);
}

static void test_two_domains_resident(Nd500Machine* m, Nd500Cpu* cpu,
                                      const char* dom_a, const char* dom_b) {
    printf("\nTwo domains resident at once:\n");

    int domain_a = -1, domain_b = -1;
    if (load_dom(m, cpu, dom_a, &domain_a) != 0) {
        printf("  [FAIL] could not load %s\n", dom_a);
        g_failures++;
        return;
    }
    int pages_a = 0;
    unsigned long digest_a = prog_image_digest(cpu, (uint8_t)domain_a, &pages_a);
    check(pages_a > 0, "domain A has a resident PROG image");

    /* A nested run: its pages must come from somewhere else entirely. */
    void* scope = nd500_segment_alloc_state_save(m);
    void* mmu = nd500_mmu_state_save();

    if (load_dom(m, cpu, dom_b, &domain_b) != 0) {
        printf("  [FAIL] could not load %s\n", dom_b);
        g_failures++;
        return;
    }
    check(domain_b != domain_a, "domain B got its own domain number");

    int pages_b = 0;
    unsigned long digest_b_now = prog_image_digest(cpu, (uint8_t)domain_b, &pages_b);
    check(pages_b > 0, "domain B has a resident PROG image");
    (void)digest_b_now;

    /* THE point of the exercise: A is still intact with B loaded. */
    int pages_a_now = 0;
    unsigned long digest_a_now = prog_image_digest(cpu, (uint8_t)domain_a, &pages_a_now);
    check(pages_a_now == pages_a, "domain A still maps the same page count");
    check(digest_a_now == digest_a, "domain A's resident image is untouched by loading B");

    /* Close the nested scope: B's pages come back, A keeps its own. */
    uint32_t free_before_pop = nd500_phys_pages_free(m);
    nd500_mmu_state_restore(mmu);
    nd500_segment_alloc_state_restore(scope);
    check(nd500_phys_pages_free(m) > free_before_pop, "closing the scope reclaimed pages");

    int pages_after = 0;
    unsigned long digest_after = prog_image_digest(cpu, (uint8_t)domain_a, &pages_after);
    check(pages_after == pages_a, "domain A still mapped after the nested scope closed");
    check(digest_after == digest_a, "domain A's image survived the nested run");
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <dom_a> <dom_b>\n", argv[0]);
        return 2;
    }

    Nd500Machine machine;
    Nd500Cpu cpu;
    nd500_machine_init(&machine, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &machine);
    machine.cpu = &cpu;

    printf("Physical allocator / concurrent domain test\n");
    test_allocator(&machine);
    test_two_domains_resident(&machine, &cpu, argv[1], argv[2]);

    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED",
           g_failures, g_failures == 1 ? "" : "s");

    nd500_machine_free(&machine);
    return g_failures ? 1 : 0;
}
