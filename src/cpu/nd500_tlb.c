/*
 * ND-500 translation cache - storage and invalidation.
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

extern unsigned long long g_tlb_hits, g_tlb_misses, g_tlb_flushes;

uint8_t g_nd500_xlat_bm[ND500_TLB_XLAT_PAGES / 8];
int     g_nd500_tlb_on = 0;

TlbEntry g_nd500_tlb[ND500_TLB_SIZE];
int      g_nd500_tlb_init = 0;

void nd500_mmu_tlb_flush(void) {
	if (!g_nd500_tlb_init) return;
	g_tlb_flushes++;
	for (uint32_t i = 0; i < ND500_TLB_SIZE; i++)
		g_nd500_tlb[i].tag = ND500_TLB_EMPTY;
}

void nd500_mmu_tlb_init_once(void) {
	if (g_nd500_tlb_init) return;
	g_nd500_tlb_init = 1;
	for (uint32_t i = 0; i < ND500_TLB_SIZE; i++)
		g_nd500_tlb[i].tag = ND500_TLB_EMPTY;
	const char* e = getenv("ND500X_NOTLB");
	g_nd500_tlb_on = (e && e[0] && e[0] != '0') ? 0 : 1;
	nd500_mmu_tlb_stat_install();
	if (!g_nd500_tlb_on)
		printf("ND-500: translation cache DISABLED (ND500X_NOTLB)\n");
}

/* ND500X_TLBSTAT=1: hit/miss/flush counts at exit. A translation cache that
 * is flushed constantly is worse than none, so this is the number that
 * decides whether the design works, not a guess about it. */
unsigned long long g_tlb_hits, g_tlb_misses, g_tlb_flushes;
static void tlb_report(void) {
	const char* e = getenv("ND500X_TLBSTAT");
	if (!e || !e[0] || e[0] == '0') return;
	unsigned long long tot = g_tlb_hits + g_tlb_misses;
	fprintf(stderr, "[TLBSTAT] hits=%llu misses=%llu (%.1f%% hit) flushes=%llu\n",
	        g_tlb_hits, g_tlb_misses,
	        tot ? 100.0 * (double)g_tlb_hits / (double)tot : 0.0, g_tlb_flushes);
}
void nd500_mmu_tlb_stat_install(void) { atexit(tlb_report); }
