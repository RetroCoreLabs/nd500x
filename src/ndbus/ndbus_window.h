/*
 * ndbus_window.h - the ND-100's WORD-addressed view of the byte-addressed pool
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * ONE backing array, TWO ports. The ND-5000 runs out of the pool byte by byte;
 * the ND-100 sees the same memory through an MPM-5 window as 16-bit WORDS. This
 * file is the conversion between those two views, in one place, because getting
 * it wrong produces memory that reads back as zero or as the neighbouring cell
 * and nothing says why.
 *
 * THE THREE RULES
 *
 * 1. WORD OFFSET TIMES TWO IS BYTE OFFSET. The ND-100 is word-addressed
 *    throughout - its physical path builds `((ppn << 10) | dip)`, a WORD address
 *    - and the pool is byte-addressed. Word N of the window is bytes 2N and
 *    2N+1. Classifying or indexing with a byte address where a word address
 *    belongs loses the upper half of the window; that regression has been made
 *    before, in the ND-100 bank table.
 *
 * 2. BIG-ENDIAN, HIGH BYTE FIRST. Byte 2N is the high half of the word. Both
 *    machines see it that way - the ND-500 is big-endian throughout, and the
 *    ND-100 window presents the same byte pairs - so there is no swap anywhere
 *    and a host's own endianness never enters into it.
 *
 * 3. THE OFFSET IS INTO THE WINDOW, NOT THE MACHINE. Callers pass an offset
 *    relative to the start of the window. Where the window sits in ND-100
 *    physical memory is the bank table's business, and where it sits in the pool
 *    is this struct's; a caller that had to know both would get one of them
 *    wrong.
 */

#ifndef NDBUS_WINDOW_H
#define NDBUS_WINDOW_H

#include "ndbus_pool.h"

typedef struct NdbusWindow
{
    NdbusPool *pool;
    uint32_t   pool_byte_base; /* where this window starts IN THE POOL */
    uint32_t   length_word;    /* window size in ND-100 WORDS */
} NdbusWindow;

/*
 * Attach a window covering `length_word` 16-bit words starting at
 * `pool_byte_base` in the pool. Returns false when that range does not fit the
 * pool - refused rather than clamped, because a window that silently ends early
 * gives the ND-100 memory that reads as zero from some address onward.
 */
bool ndbus_window_attach(NdbusWindow *window, NdbusPool *pool, uint32_t pool_byte_base,
                         uint32_t length_word);

/* One word out of the window. Outside it reads 0. */
uint16_t ndbus_window_read_word(const NdbusWindow *window, uint32_t word_offset);

/* One word into the window. Outside it is refused and writes nothing. */
bool ndbus_window_write_word(NdbusWindow *window, uint32_t word_offset, uint16_t value);

/*
 * Half-word writes, for the ND-100's WRITEMODE_MSB and WRITEMODE_LSB.
 *
 * MSB is the HIGH byte of the word, which is pool byte 2N - the FIRST of the
 * pair, not the second. Writing the wrong half of a big-endian word is the kind
 * of mistake that leaves a program counter or a page-table entry off by 256 and
 * looks like a random fault much later.
 */
bool ndbus_window_write_msb(NdbusWindow *window, uint32_t word_offset, uint8_t value);
bool ndbus_window_write_lsb(NdbusWindow *window, uint32_t word_offset, uint8_t value);

#endif /* NDBUS_WINDOW_H */
