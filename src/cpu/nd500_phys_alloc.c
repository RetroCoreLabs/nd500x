/*
 * nd500_phys_alloc.c - physical page allocator (see nd500_phys_alloc.h)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "nd500_phys_alloc.h"
#include "nd500_tlb.h"
#include "../machine/machine_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NBPG
#define NBPG    2048        /* bytes per page frame */
#endif
#ifndef PGSHIFT
#define PGSHIFT 11          /* LOG2(NBPG) */
#endif

/* Maximum nesting depth. A domain starting a domain is a couple of levels in
 * practice; the limit exists so a runaway push cannot grow without bound. */
#define ARENA_DEPTH_MAX 64

typedef struct {
    uint32_t  page_count;
    uint32_t* owner;        /* one tag per page frame */
    uint32_t  hint;         /* first PFN worth scanning from */

    /* First PFN of the guest's own page pool; 0 = no guest pool declared. The
     * allocator never hands out a page at or above it. See
     * nd500_phys_set_guest_pool_base() in the header for why. */
    uint32_t  guest_pool_pfn;

    /* Arena ids are handed out from a counter that only ever increases, so an
     * id is never reused. That matters: a caller may pop the same id twice (an
     * error path that also runs the normal cleanup), and with recycled ids the
     * second pop would silently free whatever scope had since taken the number.
     * `open` is the stack of currently open ids - a pop of an id that is not on
     * it does nothing. */
    uint32_t  open[ARENA_DEPTH_MAX];
    int       depth;
    uint32_t  next_id;
} PhysAlloc;

/* Lazily build the map from the machine's current memory size. Returns NULL if
 * the machine has no memory yet. */
static PhysAlloc* pa_get(Nd500Machine* m) {
    if (!m || !m->memory || m->memory_size < NBPG) return NULL;

    PhysAlloc* pa = (PhysAlloc*)m->phys_alloc;
    uint32_t pages = m->memory_size >> PGSHIFT;

    if (pa && pa->page_count == pages) return pa;

    if (pa) {
        /* Memory was resized - grow/shrink the map, keeping existing tags. */
        uint32_t* no = (uint32_t*)realloc(pa->owner, pages * sizeof(uint32_t));
        if (!no) return NULL;
        if (pages > pa->page_count) {
            memset(no + pa->page_count, 0,
                   (pages - pa->page_count) * sizeof(uint32_t));
        }
        pa->owner = no;
        pa->page_count = pages;
        if (pa->hint >= pages) pa->hint = 1;
        return pa;
    }

    pa = (PhysAlloc*)calloc(1, sizeof(PhysAlloc));
    if (!pa) return NULL;
    pa->owner = (uint32_t*)calloc(pages, sizeof(uint32_t));
    if (!pa->owner) { free(pa); return NULL; }
    pa->page_count = pages;
    pa->hint       = 1;
    pa->depth      = 0;
    pa->next_id    = ND500_PHYS_ARENA_FIRST;
    /* PFN 0 is never allocatable: a PTE with PFN 0 means "not present". */
    pa->owner[0] = ND500_PHYS_PERM;
    m->phys_alloc = pa;
    return pa;
}

void nd500_phys_alloc_reset(Nd500Machine* m) {
    if (!m) return;
    PhysAlloc* pa = (PhysAlloc*)m->phys_alloc;
    if (!pa) return;
    free(pa->owner);
    free(pa);
    m->phys_alloc = NULL;
}

/* Tag charged to a new allocation: the innermost open arena, else PERMANENT. */
static uint32_t pa_current_owner(const PhysAlloc* pa) {
    return pa->depth > 0 ? pa->open[pa->depth - 1] : ND500_PHYS_PERM;
}

static void pa_zero_pages(Nd500Machine* m, uint32_t base_pfn, uint32_t count) {
    uint64_t base = (uint64_t)base_pfn << PGSHIFT;
    uint64_t len  = (uint64_t)count * NBPG;
    if (base + len > m->memory_size) return;   /* caller already bounds-checked */
    /* Bypasses nd500_bus_write8, so flush by hand - the page being zeroed may be a table a walk already read. */
    nd500_mmu_tlb_flush();
    memset(m->memory + base, 0, (size_t)len);
}

uint32_t nd500_phys_alloc_pages(Nd500Machine* m, uint32_t count, int zero) {
    PhysAlloc* pa = pa_get(m);
    if (!pa || count == 0) return 0;

    uint32_t tag = pa_current_owner(pa);

    /* Pages at or above the guest pool base are the guest's, not ours. */
    uint32_t limit = pa->page_count;
    if (pa->guest_pool_pfn && pa->guest_pool_pfn < limit) limit = pa->guest_pool_pfn;

    /* First fit over a contiguous free run. The map is small (8192 entries for a
     * 16 MB machine) and allocation happens at load/segment-create time only. */
    uint32_t start = (pa->hint < 1) ? 1 : pa->hint;
    for (int pass = 0; pass < 2; pass++) {
        uint32_t run = 0;
        for (uint32_t pfn = start; pfn < limit; pfn++) {
            if (pa->owner[pfn] != ND500_PHYS_FREE) { run = 0; continue; }
            if (++run < count) continue;

            uint32_t base = pfn - count + 1;
            for (uint32_t i = 0; i < count; i++) pa->owner[base + i] = tag;
            pa->hint = pfn + 1;
            if (zero) pa_zero_pages(m, base, count);
            return base;
        }
        /* Second pass: the hint may have skipped a hole freed behind us. */
        if (start == 1) break;
        start = 1;
    }
    /* Say WHY, and say it always: a silent 0 here used to be indistinguishable
     * from "out of memory" on a 16 MB machine that still had 14 MB free. */
    if (pa->guest_pool_pfn && limit < pa->page_count) {
        fprintf(stderr, "[PHYS] out of emulator-private memory: %u page(s) wanted, "
                        "window is 0x%08X..0x%08X (guest pool starts there)\n",
                count, 1u << PGSHIFT, limit << PGSHIFT);
    }
    return 0;
}

void nd500_phys_set_guest_pool_base(Nd500Machine* m, uint32_t byte_base) {
    PhysAlloc* pa = pa_get(m);
    if (!pa) return;
    pa->guest_pool_pfn = byte_base >> PGSHIFT;
}

uint32_t nd500_phys_guest_pool_base(Nd500Machine* m) {
    PhysAlloc* pa = pa_get(m);
    return pa ? (pa->guest_pool_pfn << PGSHIFT) : 0u;
}

void nd500_phys_free_pages(Nd500Machine* m, uint32_t base_pfn, uint32_t count) {
    PhysAlloc* pa = pa_get(m);
    if (!pa || count == 0 || base_pfn == 0) return;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t pfn = base_pfn + i;
        if (pfn >= pa->page_count) break;
        pa->owner[pfn] = ND500_PHYS_FREE;
    }
    if (base_pfn < pa->hint) pa->hint = base_pfn;
}

int nd500_phys_reserve(Nd500Machine* m, uint32_t byte_base, uint32_t byte_len) {
    PhysAlloc* pa = pa_get(m);
    if (!pa || byte_len == 0) return -1;

    uint32_t first = byte_base >> PGSHIFT;
    uint32_t last  = (byte_base + byte_len - 1) >> PGSHIFT;
    if (last >= pa->page_count) return -1;

    /* A page already charged to an arena belongs to a live domain; silently
     * marking it permanent would hand the same memory to two owners. */
    for (uint32_t pfn = first; pfn <= last; pfn++) {
        if (pa->owner[pfn] >= ND500_PHYS_ARENA_FIRST) return -1;
    }
    for (uint32_t pfn = first; pfn <= last; pfn++) {
        pa->owner[pfn] = ND500_PHYS_PERM;
    }
    if (pa->hint <= last) pa->hint = last + 1;
    return 0;
}

uint32_t nd500_phys_arena_push(Nd500Machine* m) {
    PhysAlloc* pa = pa_get(m);
    if (!pa) return 0;
    if (pa->depth >= ARENA_DEPTH_MAX) return 0;
    uint32_t id = pa->next_id++;
    pa->open[pa->depth++] = id;
    return id;
}

void nd500_phys_arena_pop(Nd500Machine* m, uint32_t id) {
    PhysAlloc* pa = pa_get(m);
    if (!pa || id < ND500_PHYS_ARENA_FIRST) return;

    /* Only an id that is actually open may be popped - popping the same id
     * twice must not reach into whatever scope is open now. */
    int at = -1;
    for (int i = 0; i < pa->depth; i++) {
        if (pa->open[i] == id) { at = i; break; }
    }
    if (at < 0) return;

    /* Ids only increase, so everything allocated at or inside this scope
     * carries a tag >= id. */
    uint32_t lowest_freed = pa->page_count;
    for (uint32_t pfn = 1; pfn < pa->page_count; pfn++) {
        if (pa->owner[pfn] >= id) {
            pa->owner[pfn] = ND500_PHYS_FREE;
            if (pfn < lowest_freed) lowest_freed = pfn;
        }
    }
    if (lowest_freed < pa->hint) pa->hint = lowest_freed;
    pa->depth = at;
}

uint32_t nd500_phys_pages_free(Nd500Machine* m) {
    PhysAlloc* pa = pa_get(m);
    if (!pa) return 0;
    uint32_t n = 0;
    for (uint32_t pfn = 0; pfn < pa->page_count; pfn++) {
        if (pa->owner[pfn] == ND500_PHYS_FREE) n++;
    }
    return n;
}

uint32_t nd500_phys_pages_total(Nd500Machine* m) {
    PhysAlloc* pa = pa_get(m);
    return pa ? pa->page_count : 0;
}

uint32_t nd500_phys_page_owner(Nd500Machine* m, uint32_t pfn) {
    PhysAlloc* pa = pa_get(m);
    if (!pa || pfn >= pa->page_count) return ND500_PHYS_FREE;
    return pa->owner[pfn];
}

uint32_t nd500_phys_high_water_pfn(Nd500Machine* m) {
    PhysAlloc* pa = pa_get(m);
    if (!pa) return 0;
    /* Scan down for the topmost page that is not free. A freed page below it
     * still counts as territory the emulator has used, which is the point:
     * the reservation has to cover the peak, not the current occupancy. */
    for (uint32_t p = pa->page_count; p > 0; p--) {
        if (pa->owner[p - 1] != ND500_PHYS_FREE) return p;
    }
    return 0;
}

void nd500_phys_alloc_report(Nd500Machine* m) {
    PhysAlloc* pa = pa_get(m);
    if (!pa) { fprintf(stderr, "[PHYS] allocator not initialised\n"); return; }

    uint32_t used = 0, perm = 0;
    for (uint32_t p = 0; p < pa->page_count; p++) {
        if (pa->owner[p] == ND500_PHYS_FREE) continue;
        used++;
        if (pa->owner[p] == ND500_PHYS_PERM) perm++;
    }
    uint32_t hw = nd500_phys_high_water_pfn(m);
    fprintf(stderr,
            "[PHYS] pages: %u used (%u permanent) of %u; high-water pfn %u"
            " = phys 0x%08X (%u KB)\n",
            used, perm, pa->page_count, hw, hw << PGSHIFT,
            (hw << PGSHIFT) / 1024u);
}
