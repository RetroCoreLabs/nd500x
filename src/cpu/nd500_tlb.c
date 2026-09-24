/*
 * nd500_tlb.c - translation cache (TSB / TLB): storage and invalidation
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Deliberately its OWN translation unit with no dependency beyond stdlib:
 * nd500_phys_alloc.c, machine_loader.c and debug_api.c all need to flush, and
 * if the flush lived in nd500_mmu.c every target that links libnd500_cpu
 * would drag that object - and its debug_api references - in with it.
 *
 * The design, and why this is safe, is documented in nd500_tlb.h.
 */
#include "nd500_tlb.h"
#include <stdlib.h>
#include <stdio.h>
#include "nd500_settings.h"   /* emulator knobs, as plain fields */

extern unsigned long long g_tlb_hits, g_tlb_misses, g_tlb_flushes;

/* Defined at the bottom of this file but called from tlb_init_once() above it.
 * GCC 11 let the implicit declaration slide with a warning; GCC 14 and later
 * make it a hard error, so say it properly. */
void nd500_mmu_tlb_stat_install(void);

uint8_t g_nd500_xlat_bm[ND500_TLB_XLAT_PAGES / 8];

/*
 * The registry of attached caches.
 *
 * Kept here rather than reached through Nd500Cpu on purpose: this translation
 * unit deliberately has no dependency beyond stdlib (see the file comment), and
 * the coarse flush callers - machine_loader.c, nd500_phys_alloc.c, debug_api.c -
 * have no CPU pointer in hand at all.
 */
static Nd500Tlb *s_tlbs[ND500_TLB_MAX_CPUS];
static int       s_tlb_count;

int g_nd500_tlb_active;

/* Recompute the hot-path gate: non-zero while any registered cache is on. */
static void tlb_recount_active(void) {
    int active = 0;
    for (int i = 0; i < s_tlb_count; i++)
        if (s_tlbs[i] != NULL && s_tlbs[i]->on)
            active = 1;
    g_nd500_tlb_active = active;
}

bool nd500_tlb_register(Nd500Tlb *tlb) {
    if (tlb == NULL) return false;
    for (int i = 0; i < s_tlb_count; i++)
        if (s_tlbs[i] == tlb) return true;          /* already attached */
    if (s_tlb_count >= ND500_TLB_MAX_CPUS) {
        /* Refused, not dropped silently: a CPU that is not in the registry
         * never receives a shootdown and would run on stale translations
         * forever, which shows up as impossible memory contents much later. */
        fprintf(stderr, "ND-500: TLB registry full (%d CPUs) - CPU not attached\n",
                ND500_TLB_MAX_CPUS);
        return false;
    }
    s_tlbs[s_tlb_count++] = tlb;
    tlb_recount_active();
    return true;
}

void nd500_tlb_unregister(Nd500Tlb *tlb) {
    for (int i = 0; i < s_tlb_count; i++) {
        if (s_tlbs[i] != tlb) continue;
        for (int j = i; j < s_tlb_count - 1; j++)
            s_tlbs[j] = s_tlbs[j + 1];
        s_tlbs[--s_tlb_count] = NULL;
        tlb_recount_active();
        return;
    }
}

void nd500_tlb_flush_one(Nd500Tlb *tlb) {
    if (tlb == NULL || !tlb->init) return;
    g_tlb_flushes++;
    for (uint32_t i = 0; i < ND500_TLB_SIZE; i++)
        tlb->entries[i].tag = ND500_TLB_EMPTY;
}

void nd500_mmu_tlb_flush(void) {
    for (int i = 0; i < s_tlb_count; i++)
        nd500_tlb_flush_one(s_tlbs[i]);
}

void nd500_tlb_signal_flush(void) {
    /* Set the flag in EVERY attached cache, INCLUDING the writer's own. The
     * writing CPU is just as capable of holding a translation it has now
     * invalidated as any other. Relaxed: the flag only has to arrive, and each
     * CPU re-reads it at the top of every instruction. */
    for (int i = 0; i < s_tlb_count; i++)
        if (s_tlbs[i] != NULL)
            __atomic_store_n(&s_tlbs[i]->flush_pending, 1u, __ATOMIC_RELAXED);
}

void nd500_tlb_init_once(Nd500Tlb *tlb) {
    if (tlb == NULL || tlb->init) return;
    tlb->init = 1;
    for (uint32_t i = 0; i < ND500_TLB_SIZE; i++)
        tlb->entries[i].tag = ND500_TLB_EMPTY;
    tlb->on = nd500_settings()->tlb_enabled;
    tlb_recount_active();
    nd500_mmu_tlb_stat_install();
    if (!tlb->on)
        printf("ND-500: translation cache DISABLED (ND500X_NOTLB)\n");
}

/* ND500X_TLBSTAT=1: hit/miss/flush counts at exit. A translation cache that
 * is flushed constantly is worse than none, so this is the number that
 * decides whether the design works, not a guess about it. */
unsigned long long g_tlb_hits, g_tlb_misses, g_tlb_flushes;
static void tlb_report(void) {
    if (!nd500_settings()->tlbstat) return;
    unsigned long long tot = g_tlb_hits + g_tlb_misses;
    fprintf(stderr, "[TLBSTAT] hits=%llu misses=%llu (%.1f%% hit) flushes=%llu\n",
            g_tlb_hits, g_tlb_misses,
            tot ? 100.0 * (double)g_tlb_hits / (double)tot : 0.0, g_tlb_flushes);
}
void nd500_mmu_tlb_stat_install(void) { atexit(tlb_report); }
