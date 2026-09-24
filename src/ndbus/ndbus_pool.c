/*
 * ndbus_pool.c - the shared MPM-5 memory pool
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include <stdlib.h>
#include <string.h>

#include "ndbus_pool.h"

bool ndbus_pool_create(NdbusPool *pool, uint32_t size_bytes)
{
    if (pool == NULL)
    {
        return false;
    }
    pool->bytes = NULL;
    pool->size  = 0;

    if (size_bytes == 0)
    {
        return false;
    }

    uint8_t *bytes = (uint8_t *)calloc((size_t)size_bytes, 1);
    if (bytes == NULL)
    {
        return false;
    }

    pool->bytes = bytes;
    pool->size  = size_bytes;
    return true;
}

void ndbus_pool_destroy(NdbusPool *pool)
{
    if (pool == NULL)
    {
        return;
    }
    free(pool->bytes);
    pool->bytes = NULL;
    pool->size  = 0;
}

bool ndbus_pool_contains(const NdbusPool *pool, uint32_t offset, uint32_t length)
{
    if (pool == NULL || pool->bytes == NULL)
    {
        return false;
    }
    /* 64-bit sum so a range that wraps 2^32 is refused, not accepted. */
    return ((uint64_t)offset + (uint64_t)length) <= (uint64_t)pool->size;
}

uint8_t ndbus_pool_read8(const NdbusPool *pool, uint32_t offset)
{
    if (!ndbus_pool_contains(pool, offset, 1))
    {
        return 0;
    }
    return ndbus_pool_r8(&pool->bytes[offset]);
}

bool ndbus_pool_write8(NdbusPool *pool, uint32_t offset, uint8_t value)
{
    if (!ndbus_pool_contains(pool, offset, 1))
    {
        return false;
    }
    ndbus_pool_w8(&pool->bytes[offset], value);
    return true;
}

uint16_t ndbus_pool_read16(const NdbusPool *pool, uint32_t offset)
{
    if (!ndbus_pool_contains(pool, offset, 2))
    {
        return 0;
    }
    uint8_t hi = ndbus_pool_r8(&pool->bytes[offset]);
    uint8_t lo = ndbus_pool_r8(&pool->bytes[offset + 1]);
    return (uint16_t)(((uint16_t)hi << 8) | (uint16_t)lo);
}

bool ndbus_pool_write16(NdbusPool *pool, uint32_t offset, uint16_t value)
{
    /* Checked as a WHOLE range first: a write that would straddle the end of the
     * pool writes nothing at all, rather than the one byte that fits. */
    if (!ndbus_pool_contains(pool, offset, 2))
    {
        return false;
    }
    ndbus_pool_w8(&pool->bytes[offset], (uint8_t)(value >> 8));
    ndbus_pool_w8(&pool->bytes[offset + 1], (uint8_t)(value & 0xFF));
    return true;
}

uint32_t ndbus_pool_read32(const NdbusPool *pool, uint32_t offset)
{
    if (!ndbus_pool_contains(pool, offset, 4))
    {
        return 0;
    }
    /* Two big-endian words, high word first - the ND double order. */
    uint32_t hi = ndbus_pool_read16(pool, offset);
    uint32_t lo = ndbus_pool_read16(pool, offset + 2);
    return (hi << 16) | lo;
}

bool ndbus_pool_write32(NdbusPool *pool, uint32_t offset, uint32_t value)
{
    if (!ndbus_pool_contains(pool, offset, 4))
    {
        return false;
    }
    (void)ndbus_pool_write16(pool, offset, (uint16_t)(value >> 16));
    (void)ndbus_pool_write16(pool, offset + 2, (uint16_t)(value & 0xFFFF));
    return true;
}

bool ndbus_pool_read_bytes(const NdbusPool *pool, uint32_t offset, void *dst, uint32_t length)
{
    if (dst == NULL || !ndbus_pool_contains(pool, offset, length))
    {
        return false;
    }
    if (length == 0)
    {
        return true;
    }
    memcpy(dst, &pool->bytes[offset], (size_t)length);
    return true;
}

bool ndbus_pool_write_bytes(NdbusPool *pool, uint32_t offset, const void *src, uint32_t length)
{
    if (src == NULL || !ndbus_pool_contains(pool, offset, length))
    {
        return false;
    }
    if (length == 0)
    {
        return true;
    }
    memcpy(&pool->bytes[offset], src, (size_t)length);
    return true;
}
