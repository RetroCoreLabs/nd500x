/*
 * ndbus_window.c - the ND-100's word view of the pool
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "ndbus_window.h"

/* The one place word offsets become byte offsets. */
static inline uint64_t window_byte(const NdbusWindow *window, uint32_t word_offset)
{
    return (uint64_t)window->pool_byte_base + ((uint64_t)word_offset * 2u);
}

static bool window_contains(const NdbusWindow *window, uint32_t word_offset)
{
    return window != NULL && window->pool != NULL && word_offset < window->length_word;
}

bool ndbus_window_attach(NdbusWindow *window, NdbusPool *pool, uint32_t pool_byte_base,
                         uint32_t length_word)
{
    if (window == NULL)
    {
        return false;
    }
    window->pool = NULL;
    window->pool_byte_base = 0;
    window->length_word = 0;

    if (pool == NULL || length_word == 0)
    {
        return false;
    }

    /* 64-bit throughout: a window of 4 MW is 8 MB of byte offsets, and the
     * product overflows 32 bits long before the pool does. */
    uint64_t bytes = (uint64_t)length_word * 2u;
    if (!ndbus_pool_contains(pool, pool_byte_base, 0) ||
        (uint64_t)pool_byte_base + bytes > (uint64_t)pool->size)
    {
        /* Refused, not clamped: a window that quietly ends early hands the
         * ND-100 memory that reads as zero from some address onward, which looks
         * like a guest bug rather than a configuration one. */
        return false;
    }

    window->pool = pool;
    window->pool_byte_base = pool_byte_base;
    window->length_word = length_word;
    return true;
}

uint16_t ndbus_window_read_word(const NdbusWindow *window, uint32_t word_offset)
{
    if (!window_contains(window, word_offset))
    {
        return 0;
    }
    /* Big-endian, high byte first - the same pair the ND-5000 sees. */
    return ndbus_pool_read16(window->pool, (uint32_t)window_byte(window, word_offset));
}

bool ndbus_window_write_word(NdbusWindow *window, uint32_t word_offset, uint16_t value)
{
    if (!window_contains(window, word_offset))
    {
        return false;
    }
    return ndbus_pool_write16(window->pool, (uint32_t)window_byte(window, word_offset), value);
}

bool ndbus_window_write_msb(NdbusWindow *window, uint32_t word_offset, uint8_t value)
{
    if (!window_contains(window, word_offset))
    {
        return false;
    }
    /* The HIGH byte is the FIRST of the pair. */
    return ndbus_pool_write8(window->pool, (uint32_t)window_byte(window, word_offset), value);
}

bool ndbus_window_write_lsb(NdbusWindow *window, uint32_t word_offset, uint8_t value)
{
    if (!window_contains(window, word_offset))
    {
        return false;
    }
    return ndbus_pool_write8(window->pool, (uint32_t)window_byte(window, word_offset) + 1u, value);
}
