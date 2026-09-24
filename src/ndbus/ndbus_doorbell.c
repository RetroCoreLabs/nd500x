/*
 * ndbus_doorbell.c - release/acquire on the doorbell handshake
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "ndbus_doorbell.h"

bool ndbus_doorbell_ring(NdbusPool *pool, uint32_t offset, uint8_t value)
{
    if (!ndbus_pool_contains(pool, offset, 1))
    {
        return false;
    }
    /* RELEASE: everything this thread wrote to the pool before now becomes
     * visible to whoever loads this cell with acquire. */
    __atomic_store_n(&pool->bytes[offset], value, __ATOMIC_RELEASE);
    return true;
}

uint8_t ndbus_doorbell_read(const NdbusPool *pool, uint32_t offset)
{
    if (!ndbus_pool_contains(pool, offset, 1))
    {
        return 0;
    }
    return __atomic_load_n(&pool->bytes[offset], __ATOMIC_ACQUIRE);
}

uint8_t ndbus_doorbell_take(NdbusPool *pool, uint32_t offset)
{
    uint8_t value = ndbus_doorbell_read(pool, offset);
    if (value != 0)
    {
        /* Relaxed: the acquire above already ordered the message bytes into this
         * thread, and no other thread clears this cell. */
        ndbus_pool_w8(&pool->bytes[offset], 0);
    }
    return value;
}
