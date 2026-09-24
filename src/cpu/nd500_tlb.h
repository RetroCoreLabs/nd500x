/*
 * nd500_tlb.h - translation cache (TSB / TLB)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
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
 * outside io.c), tests that bitmap when a write lands on one of those pages. A
 * guest PTE edit therefore invalidates the cache whether or not the guest
 * remembers to say dctsb, and so does an emulator-side edit, and so does disk
 * DMA.
 *
 * WITH SEVERAL CPUs it does not flush inline. The other CPUs run on their own
 * host threads and their entries[] must not be touched from the writer's thread,
 * so the write sets flush_pending in EVERY attached CPU and each CPU flushes its
 * own cache at the top of its next instruction. Flushing only the writing CPU's
 * cache - which is what a single-CPU emulator can get away with - would leave
 * every other CPU running on translations the page-table edit just invalidated.
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
#ifndef ND500_TLB_H
#define ND500_TLB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* One bit per 2KB physical page. 65536 pages = 128MB of physical address
 * space, comfortably past the 16MB nd500x actually boots with. Writes above
 * that range cannot be page tables we walked, so they need no check. */
#define ND500_TLB_XLAT_PAGES  65536u

/*
 * THE BITMAP IS SHARED BY EVERY CPU; THE CACHE IS NOT.
 *
 * g_nd500_xlat_bm describes the SHARED MEMORY's role - "a translation walk once
 * read a page table out of this physical page" - which is a property of the
 * memory, not of whichever CPU happened to do the walking. So it stays one
 * global bitmap, and bits are set with __atomic_fetch_or: bits are set rarely
 * (only on a walk) and read constantly (on every physical write), which is the
 * right way round for an atomic RMW.
 *
 * The CACHE ITSELF is per CPU (Nd500Tlb below). Several CPUs sharing one
 * translation cache is a wrong-answer bug before it is a performance bug: CPU 1
 * would serve CPU 2 a translation from CPU 2's own domain number in CPU 1's
 * address space.
 */
extern uint8_t g_nd500_xlat_bm[ND500_TLB_XLAT_PAGES / 8];

#define ND500_TLB_BITS  12
#define ND500_TLB_SIZE  (1u << ND500_TLB_BITS)
#define ND500_TLB_EMPTY 0xFFFFFFFFu

/* Seven ND-5000 slots on the octobus (70B..76B) plus room for the ND-100 side,
 * rounded up. Registration past this is refused, not silently ignored. */
#define ND500_TLB_MAX_CPUS 8

/* Cache line on x86-64 and AArch64. Used to keep one CPU's flush_pending off
 * every other CPU's line. */
#define ND500_CACHE_LINE 64

typedef struct {
    uint32_t tag;       /* (vpn << 9) | (domain << 1) | is_instruction */
    uint32_t pfn;       /* resulting physical page number */
    uint8_t  writable;  /* this translation was validated for a WRITE */
} TlbEntry;

/*
 * One CPU's translation cache.
 *
 * flush_pending is FIRST and alignment-padded so it sits alone on a cache line.
 * Every physical write that lands on a page-table page sets it in EVERY attached
 * CPU; without the padding the threads would ping-pong a single line on every
 * page-table write, and that costs more than the flush it is avoiding.
 *
 * flush_pending is written by other threads and read by this CPU, so it is
 * touched only through __atomic_* with relaxed ordering. Relaxed is enough: the
 * flush is a conservative crutch, and it is already far stricter than the real
 * machine, where a stale translation after another CPU edits a page table is the
 * guest's problem and is handled by the guest issuing DCTSB / PCTSB.
 */
typedef struct Nd500Tlb {
    _Alignas(ND500_CACHE_LINE) uint32_t flush_pending;
    uint8_t  pad[ND500_CACHE_LINE - sizeof(uint32_t)];

    TlbEntry entries[ND500_TLB_SIZE];
    int      on;        /* 0 while disabled (ND500X_NOTLB) or not yet primed */
    int      init;      /* entries[] have been set to ND500_TLB_EMPTY */
} Nd500Tlb;

/*
 * Non-zero while at least one registered CPU has its cache switched on. The hot
 * path in nd500_tlb_on_phys_write() reads this instead of reaching into the
 * registry, so a build with the cache disabled pays one predictable load per
 * physical byte written and nothing else.
 */
extern int g_nd500_tlb_active;

/* Attach / detach a CPU's cache. Registration is refused past
 * ND500_TLB_MAX_CPUS and returns false, rather than dropping the CPU silently
 * and leaving it stale forever. */
bool nd500_tlb_register(Nd500Tlb *tlb);
void nd500_tlb_unregister(Nd500Tlb *tlb);

/* Prime one cache: mark every entry empty and read the enable knob. Idempotent. */
void nd500_tlb_init_once(Nd500Tlb *tlb);

/* Drop every cached translation in ONE cache, immediately. */
void nd500_tlb_flush_one(Nd500Tlb *tlb);

/*
 * Drop every cached translation in EVERY registered cache, immediately.
 *
 * This is the coarse path: image load, the debugger's write-memory, the physical
 * allocator's page zeroing, an MMU enable/disable, a shadow-table edit. All of
 * them are rare events where flushing everything costs nothing, and all of them
 * can change what ANY CPU would translate, so flushing only the caller's cache
 * would be wrong.
 */
void nd500_mmu_tlb_flush(void);

/* Out-of-line half of nd500_tlb_on_phys_write(): a write landed on a page some
 * walk read a translation from, so every attached CPU must drop its cache. Sets
 * flush_pending rather than flushing inline - the other CPUs are running on
 * their own threads and their entries[] must not be touched from here. */
void nd500_tlb_signal_flush(void);

/*
 * Tested ONCE PER INSTRUCTION at the top of the step loop - never per memory
 * access - and flushes if set. Returns true if it flushed.
 */
static inline bool nd500_tlb_take_pending(Nd500Tlb *tlb) {
    if (tlb == NULL) return false;
    if (__atomic_load_n(&tlb->flush_pending, __ATOMIC_RELAXED) == 0) return false;
    __atomic_store_n(&tlb->flush_pending, 0u, __ATOMIC_RELAXED);
    nd500_tlb_flush_one(tlb);
    return true;
}

/* vpn is 21 bits, domain 8, is_instruction 1 - 30 bits total, so the tag is
 * exact: a tag match is a real match, never an aliasing accident. */
static inline uint32_t nd500_tlb_tag(uint32_t vaddr, uint8_t domain, int is_instruction) {
    return ((vaddr >> 11) << 9) | ((uint32_t)domain << 1) | (uint32_t)(is_instruction != 0);
}
static inline uint32_t nd500_tlb_slot(uint32_t tag) {
    return (tag * 2654435761u) >> (32 - ND500_TLB_BITS);   /* Knuth multiplicative */
}

/* Record that a walk read a translation structure out of this physical page,
 * so a later write to it invalidates the cache. Atomic because several CPUs
 * walk their own page tables concurrently out of the one shared memory; the OR
 * is never undone, so relaxed ordering is enough. */
static inline void nd500_tlb_note_xlat_page(uint32_t phys) {
    uint32_t pg = phys >> 11;
    if (pg < ND500_TLB_XLAT_PAGES)
        __atomic_fetch_or(&g_nd500_xlat_bm[pg >> 3], (uint8_t)(1u << (pg & 7)),
                          __ATOMIC_RELAXED);
}

/* Hot path: called for every physical byte written. Almost always a single
 * predictable load-and-test, because almost no page is a page table. */
static inline void nd500_tlb_on_phys_write(uint32_t phys) {
    if (!g_nd500_tlb_active) return;
    uint32_t pg = phys >> 11;               /* PGSHIFT */
    if (pg < ND500_TLB_XLAT_PAGES &&
        (__atomic_load_n(&g_nd500_xlat_bm[pg >> 3], __ATOMIC_RELAXED) &
         (uint8_t)(1u << (pg & 7))))
        nd500_tlb_signal_flush();
}

#endif /* ND500_TLB_H */
