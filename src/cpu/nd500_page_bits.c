/*
 * nd500_page_bits.c - PGU / WIP page bitmaps (see nd500_page_bits.h)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "nd500_page_bits.h"
#include "nd500_phys_alloc.h"
#include "../machine/machine_types.h"
#include "cpu_protos.h"    /* Nd500Cpu, for the PC in the PGUWATCH log */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nd500_settings.h"   /* emulator knobs, as plain fields */

#ifndef NBPG
#define NBPG    2048        /* bytes per page frame */
#endif
#ifndef PGSHIFT
#define PGSHIFT 11          /* LOG2(NBPG) */
#endif

/* ND-05.009.4 16.17/16.20: "Only the lower 25 bits of the bit number are
 * significant." */
#define PAGE_NO_MASK 0x01FFFFFFu

typedef struct {
    uint32_t  page_count;
    uint32_t* word[2];      /* indexed by Nd500PageTable; 32 pages per word */
    uint32_t* queried;      /* pages RPGU/RWIP have asked about at least once */
    uint32_t  words;
} PageBits;

/* Query bookkeeping. "Every query answers 1" has two very different causes:
 * the sweep only ever visited each page once (so 1 is the honest answer for a
 * freshly cleared page), or it visited pages twice and something re-marked
 * them in between. Distinct-vs-total separates those without guessing. */
static unsigned long g_query_total = 0;
static unsigned long g_query_repeat = 0;
static unsigned long g_query_distinct = 0;

/* Lowest and highest page ever marked, per table (defined early so the query
 * log in nd500_page_bits_read_bit can print the range). */
static uint32_t g_marked_lo[2] = { 0xFFFFFFFFu, 0xFFFFFFFFu };
static uint32_t g_marked_hi[2] = { 0, 0 };

/* ND500X_PGUDBG prints the summary at process exit. It is armed here rather
 * than only at nd500_machine_free() because the --ndix and --debug paths leave
 * through the debugger REPL, which never frees the machine - hooking teardown
 * alone produced no output at all on exactly the runs worth measuring. */
static Nd500Machine* g_report_machine = NULL;

/* One exit reporter for both diagnostics. They are separate concerns but share
 * a single atexit/signal registration on purpose: two handlers would each
 * install themselves with signal() and the second would silently replace the
 * first, so whichever armed last would be the only one to report. */
static void pb_emit_reports(void) {
    if (!g_report_machine) return;
    if (nd500_settings()->pgudbg)  nd500_page_bits_report(g_report_machine);
    if (nd500_settings()->physdbg) nd500_phys_alloc_report(g_report_machine);
}

static void pb_atexit_report(void) {
    pb_emit_reports();
}

/* A scripted boot is normally ended by `timeout`, i.e. SIGTERM, which runs no
 * atexit handler - the first two measurement runs produced no output at all for
 * exactly that reason. Report from the signal too, then re-raise with the
 * default disposition so the exit status still says "killed by SIGTERM".
 *
 * fprintf is not async-signal-safe. That is tolerated here because the whole
 * facility only exists when ND500X_PGUDBG or ND500X_PHYSDBG is set: with both
 * unset no handler is installed and the emulator's signal behaviour is
 * untouched. */
static void pb_signal_report(int sig) {
    pb_emit_reports();
    g_report_machine = NULL;           /* do not report twice via atexit */
    signal(sig, SIG_DFL);
    raise(sig);
}

static void pb_arm_report(Nd500Machine* m) {
    static int armed = 0;
    if (armed) { g_report_machine = m; return; }
    int want = (nd500_settings()->pgudbg || nd500_settings()->physdbg);
    if (!want) return;
    g_report_machine = m;
    armed = 1;
    atexit(pb_atexit_report);
    signal(SIGTERM, pb_signal_report);
    signal(SIGINT, pb_signal_report);
}

/* Lazily build the bitmaps from the machine's current memory size. Returns NULL
 * if the machine has no memory yet. */
static PageBits* pb_get(Nd500Machine* m) {
    if (!m || !m->memory || m->memory_size < NBPG) return NULL;

    PageBits* pb = (PageBits*)m->page_bits;
    uint32_t pages = m->memory_size >> PGSHIFT;
    uint32_t words = (pages + 31u) / 32u;

    if (pb && pb->page_count == pages) return pb;

    if (pb) {
        /* Memory was re-sized. The bits describe physical frames that may have
         * moved, so start both tables clean rather than carrying stale bits. */
        for (int t = 0; t < 2; t++) {
            uint32_t* nw = (uint32_t*)realloc(pb->word[t], words * sizeof(uint32_t));
            if (!nw) return NULL;
            memset(nw, 0, words * sizeof(uint32_t));
            pb->word[t] = nw;
        }
        pb->page_count = pages;
        pb->words = words;
        return pb;
    }

    pb = (PageBits*)calloc(1, sizeof(PageBits));
    if (!pb) return NULL;
    for (int t = 0; t < 2; t++) {
        pb->word[t] = (uint32_t*)calloc(words, sizeof(uint32_t));
        if (!pb->word[t]) {
            free(pb->word[0]);
            free(pb);
            return NULL;
        }
    }
    pb->queried = (uint32_t*)calloc(words, sizeof(uint32_t));
    if (!pb->queried) { free(pb->word[0]); free(pb->word[1]); free(pb); return NULL; }
    pb->page_count = pages;
    pb->words = words;
    m->page_bits = pb;
    pb_arm_report(m);
    return pb;
}

void nd500_page_bits_mark(Nd500Machine* m, uint32_t phys_addr, int is_write) {
    PageBits* pb = pb_get(m);
    if (!pb) return;

    uint32_t page = phys_addr >> PGSHIFT;
    if (page >= pb->page_count) return;

    /* ND500X_PGUWATCH=<decimal pfn>: report every mark of one page, with the
     * PC that caused it. "Who re-sets this bit after ZPGU cleared it" cannot be
     * answered any other way - the counters only say that it happened. */
    {
        static long watch = -2;
        static unsigned shown = 0;
        if (watch == -2) watch = nd500_settings()->pguwatch;
        if (watch >= 0 && (uint32_t)watch == page && shown < 40u) {
            shown++;
            Nd500Cpu* c = m->cpu;
            fprintf(stderr, "[PGUMARK] page=%u %s PC=0x%08X CED=%u CAD=%u\n",
                    page, is_write ? "W" : "R",
                    c ? c->PC : 0u, c ? c->CED : 0u, c ? c->CAD : 0u);
        }
    }

    pb->word[ND500_PAGE_TABLE_PGU][page >> 5] |= 1u << (page & 31u);
    if (g_marked_lo[ND500_PAGE_TABLE_PGU] == 0xFFFFFFFFu ||
        page < g_marked_lo[ND500_PAGE_TABLE_PGU]) g_marked_lo[ND500_PAGE_TABLE_PGU] = page;
    if (page > g_marked_hi[ND500_PAGE_TABLE_PGU]) g_marked_hi[ND500_PAGE_TABLE_PGU] = page;
    if (is_write) {
        pb->word[ND500_PAGE_TABLE_WIP][page >> 5] |= 1u << (page & 31u);
        if (g_marked_lo[ND500_PAGE_TABLE_WIP] == 0xFFFFFFFFu ||
            page < g_marked_lo[ND500_PAGE_TABLE_WIP]) g_marked_lo[ND500_PAGE_TABLE_WIP] = page;
        if (page > g_marked_hi[ND500_PAGE_TABLE_WIP]) g_marked_hi[ND500_PAGE_TABLE_WIP] = page;
    }
}

uint32_t nd500_page_bits_read_bit(Nd500Machine* m, Nd500PageTable table, uint32_t page) {
    PageBits* pb = pb_get(m);
    if (!pb) return 0;

    uint32_t raw = page;
    page &= PAGE_NO_MASK;
    /* "Reading bits representing non-existing memory will give a zero result." */
    uint32_t out = (page >= pb->page_count)
                 ? 0u
                 : ((pb->word[table][page >> 5] >> (page & 31u)) & 1u);

    if (page < pb->page_count && pb->queried) {
        g_query_total++;
        if ((pb->queried[page >> 5] >> (page & 31u)) & 1u) {
            g_query_repeat++;
        } else {
            pb->queried[page >> 5] |= 1u << (page & 31u);
            g_query_distinct++;
        }
    }

    /* The guest asking about page numbers the emulator never marks is
     * indistinguishable, from the outside, from the guest asking about pages
     * that genuinely were not used - both answer 0. Logging the queried number
     * is the only way to tell those apart, and the answer decides whether the
     * tables are wired to the right page-number space at all. */
    {
        static int dbg = -1;
        static unsigned long shown = 0;
        if (dbg < 0) dbg = nd500_settings()->pgudbg;
        if (dbg && shown < 24u) {
            shown++;
            fprintf(stderr, "[PGUQ] %s page=%u (raw 0x%X) -> %u  [marked range %u..%u]\n",
                    table == ND500_PAGE_TABLE_PGU ? "RPGU" : "RWIP",
                    page, raw, out, g_marked_lo[table], g_marked_hi[table]);
        }
    }
    return out;
}

uint32_t nd500_page_bits_read_group(Nd500Machine* m, Nd500PageTable table, uint32_t group) {
    PageBits* pb = pb_get(m);
    if (!pb) return 0;

    uint32_t base = (group & PAGE_NO_MASK) * 16u;
    uint32_t result = 0;
    for (uint32_t k = 0; k < 16u; k++) {
        uint32_t page = base + k;
        if (page >= pb->page_count) break;   /* non-existing memory reads as 0 */
        if ((pb->word[table][page >> 5] >> (page & 31u)) & 1u) {
            result |= 1u << k;
        }
    }
    return result;
}

void nd500_page_bits_clear_bit(Nd500Machine* m, Nd500PageTable table, uint32_t page) {
    PageBits* pb = pb_get(m);
    if (!pb) return;

    page &= PAGE_NO_MASK;
    if (page >= pb->page_count) return;

    pb->word[table][page >> 5] &= ~(1u << (page & 31u));
}

void nd500_page_bits_set_bit(Nd500Machine* m, Nd500PageTable table, uint32_t page) {
    PageBits* pb = pb_get(m);
    if (!pb) return;

    page &= PAGE_NO_MASK;
    if (page >= pb->page_count) return;

    pb->word[table][page >> 5] |= 1u << (page & 31u);
}

void nd500_page_bits_clear_all(Nd500Machine* m, Nd500PageTable table) {
    PageBits* pb = pb_get(m);
    if (!pb) return;

    memset(pb->word[table], 0, pb->words * sizeof(uint32_t));
}

/* Instruction-execution counters. Process-wide rather than per-machine: they
 * answer "did this run reach the swap path", and a run has one machine. */
static unsigned long g_op_count[ND500_PAGE_OP_COUNT];

static const char* const g_op_name[ND500_PAGE_OP_COUNT] = {
    "RPGU", "RWIP", "ZPGU", "ZWIP", "CPGU", "CWIP"
};

void nd500_page_bits_count(Nd500PageOp op) {
    if ((int)op >= 0 && (int)op < ND500_PAGE_OP_COUNT) g_op_count[op]++;
}

void nd500_page_bits_report(Nd500Machine* m) {
    unsigned long set[2] = { 0, 0 };
    PageBits* pb = pb_get(m);
    if (pb) {
        for (int t = 0; t < 2; t++) {
            for (uint32_t w = 0; w < pb->words; w++) {
                uint32_t v = pb->word[t][w];
                while (v) { set[t]++; v &= v - 1u; }
            }
        }
    }

    fprintf(stderr, "[PGU] instruction counts:");
    for (int i = 0; i < ND500_PAGE_OP_COUNT; i++) {
        fprintf(stderr, " %s=%lu", g_op_name[i], g_op_count[i]);
    }
    fprintf(stderr, "\n[PGU] queries: %lu total, %lu distinct pages, %lu repeats\n",
            g_query_total, g_query_distinct, g_query_repeat);
    fprintf(stderr, "[PGU] pages marked: PGU=%lu WIP=%lu of %lu frames\n",
            set[ND500_PAGE_TABLE_PGU], set[ND500_PAGE_TABLE_WIP],
            pb ? (unsigned long)pb->page_count : 0ul);
}

void nd500_page_bits_reset(Nd500Machine* m) {
    if (!m || !m->page_bits) return;

    PageBits* pb = (PageBits*)m->page_bits;
    free(pb->word[0]);
    free(pb->word[1]);
    free(pb->queried);
    free(pb);
    m->page_bits = NULL;
    if (g_report_machine == m) g_report_machine = NULL;   /* no use-after-free at exit */
}
