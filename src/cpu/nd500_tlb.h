/*
 * ND-500 translation cache (TSB / TLB)
 *
 * The ND-500 has one for real: DCTSB and PCTSB are instructions whose whole
 * job is to clear it, and the NDIX kernel issues them from locore.c, machdep.c
 * and vm_page.c after it edits PTEs, the PST or a capability. So caching
 * translations is what the hardware does and what the guest is written for.
 *
 * Correctness here does NOT rely on the guest getting those flushes right.
 * locore.s says as much about the 1988 original:
 *
 *     #    This is not consistent, and was partially solved by a dctsb on
 *     #    the return from trap and a dctsb before the lcntxt at the end
 *
 * so instead the cache defends itself. Every page a walk reads a translation
 * structure from - the DIT, the PST, an L1 or L2 page table - is recorded in
 * g_nd500_xlat_bm. nd500_bus_write8, which every single physical store in the
 * emulator funnels through (verified: no *->memory[..] = assignment exists
 * outside io.c), tests that bitmap and flushes the whole cache when a write
 * lands on one of those pages. A guest PTE edit therefore invalidates the
 * cache whether or not the guest remembers to say dctsb, and so does an
 * emulator-side edit, and so does disk DMA.
 *
 * The few bulk writers that go straight at m->memory rather than through the
 * bus - image load, the debugger's write-memory, the physical allocator's
 * page zeroing - call nd500_mmu_tlb_flush() directly. They are all coarse,
 * rare events where a full flush costs nothing.
 *
 * Only SUCCESSFUL full walks are cached. Every fault path returns before the
 * fill, so a page fault is always re-walked and always re-raised - the cache
 * can never swallow a trap, and it can never manufacture one.
 *
 * Set ND500X_NOTLB=1 to disable it entirely and go back to walking every
 * access. Behaviour must be identical either way; that is the standing test.
 */
#pragma once
#include <stdint.h>

/* One bit per 2KB physical page. 65536 pages = 128MB of physical address
 * space, comfortably past the 16MB nd500x actually boots with. Writes above
 * that range cannot be page tables we walked, so they need no check. */
#define ND500_TLB_XLAT_PAGES  65536u

extern uint8_t g_nd500_xlat_bm[ND500_TLB_XLAT_PAGES / 8];
extern int     g_nd500_tlb_on;      /* 0 while disabled or not yet primed */

#define ND500_TLB_BITS  12
#define ND500_TLB_SIZE  (1u << ND500_TLB_BITS)
#define ND500_TLB_EMPTY 0xFFFFFFFFu

typedef struct {
    uint32_t tag;       /* (vpn << 9) | (domain << 1) | is_instruction */
    uint32_t pfn;       /* resulting physical page number */
    uint8_t  writable;  /* this translation was validated for a WRITE */
} TlbEntry;

extern TlbEntry g_nd500_tlb[ND500_TLB_SIZE];
extern int      g_nd500_tlb_init;

/* Drop every cached translation. Cheap and safe to call at any time. */
void nd500_mmu_tlb_flush(void);
void nd500_mmu_tlb_init_once(void);

/* vpn is 21 bits, domain 8, is_instruction 1 - 30 bits total, so the tag is
 * exact: a tag match is a real match, never an aliasing accident. */
static inline uint32_t nd500_tlb_tag(uint32_t vaddr, uint8_t domain, int is_instruction) {
    return ((vaddr >> 11) << 9) | ((uint32_t)domain << 1) | (uint32_t)(is_instruction != 0);
}
static inline uint32_t nd500_tlb_slot(uint32_t tag) {
    return (tag * 2654435761u) >> (32 - ND500_TLB_BITS);   /* Knuth multiplicative */
}

/* Record that a walk read a translation structure out of this physical page,
 * so a later write to it invalidates the cache. */
static inline void nd500_tlb_note_xlat_page(uint32_t phys) {
    uint32_t pg = phys >> 11;
    if (pg < ND500_TLB_XLAT_PAGES)
        g_nd500_xlat_bm[pg >> 3] |= (uint8_t)(1u << (pg & 7));
}

/* Hot path: called for every physical byte written. Almost always a single
 * predictable load-and-test, because almost no page is a page table. */
static inline void nd500_tlb_on_phys_write(uint32_t phys) {
    if (!g_nd500_tlb_on) return;
    uint32_t pg = phys >> 11;               /* PGSHIFT */
    if (pg < ND500_TLB_XLAT_PAGES &&
        (g_nd500_xlat_bm[pg >> 3] & (uint8_t)(1u << (pg & 7))))
        nd500_mmu_tlb_flush();
}
