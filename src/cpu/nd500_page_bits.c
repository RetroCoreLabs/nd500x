/*
 * ND-500 PGU / WIP page bitmaps - see nd500_page_bits.h for the rationale.
 */

#include "nd500_page_bits.h"
#include "../machine/machine_types.h"

#include <stdlib.h>
#include <string.h>

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
    uint32_t  words;
} PageBits;

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
    pb->page_count = pages;
    pb->words = words;
    m->page_bits = pb;
    return pb;
}

void nd500_page_bits_mark(Nd500Machine* m, uint32_t phys_addr, int is_write) {
    PageBits* pb = pb_get(m);
    if (!pb) return;

    uint32_t page = phys_addr >> PGSHIFT;
    if (page >= pb->page_count) return;

    pb->word[ND500_PAGE_TABLE_PGU][page >> 5] |= 1u << (page & 31u);
    if (is_write) {
        pb->word[ND500_PAGE_TABLE_WIP][page >> 5] |= 1u << (page & 31u);
    }
}

uint32_t nd500_page_bits_read_bit(Nd500Machine* m, Nd500PageTable table, uint32_t page) {
    PageBits* pb = pb_get(m);
    if (!pb) return 0;

    page &= PAGE_NO_MASK;
    /* "Reading bits representing non-existing memory will give a zero result." */
    if (page >= pb->page_count) return 0;

    return (pb->word[table][page >> 5] >> (page & 31u)) & 1u;
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

void nd500_page_bits_clear_all(Nd500Machine* m, Nd500PageTable table) {
    PageBits* pb = pb_get(m);
    if (!pb) return;

    memset(pb->word[table], 0, pb->words * sizeof(uint32_t));
}

void nd500_page_bits_reset(Nd500Machine* m) {
    if (!m || !m->page_bits) return;

    PageBits* pb = (PageBits*)m->page_bits;
    free(pb->word[0]);
    free(pb->word[1]);
    free(pb);
    m->page_bits = NULL;
}
