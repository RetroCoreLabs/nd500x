/*
 * ndbus_lock.c - the bus-wide lock-cycle mutex and the TSET cycle
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "ndbus_lock.h"

#ifndef __EMSCRIPTEN__
#include <pthread.h>

/* One mutex for the whole bus. See the header for why it is not one per address.
 * Statically initialised: there is no ndbus_lock_init(), so no start-up order to
 * get wrong and nothing to forget in a test. */
static pthread_mutex_t s_bus_mutex = PTHREAD_MUTEX_INITIALIZER;

void ndbus_lock(void)
{
    (void)pthread_mutex_lock(&s_bus_mutex);
}

void ndbus_unlock(void)
{
    (void)pthread_mutex_unlock(&s_bus_mutex);
}

#else /* __EMSCRIPTEN__ */

/* One WebAssembly memory, no threads: there is nothing to serialize. The calls
 * stay in the source so native and browser builds are the same code. */
void ndbus_lock(void)
{
}

void ndbus_unlock(void)
{
}

#endif

bool ndbus_tset32(NdbusPool *pool, uint32_t offset, uint32_t *out_previous)
{
    if (out_previous != NULL)
    {
        *out_previous = 0;
    }
    if (!ndbus_pool_contains(pool, offset, 4))
    {
        return false;
    }

    ndbus_lock();
    uint32_t previous = ndbus_pool_read32(pool, offset);
    bool     taken    = (previous == 0);
    if (taken)
    {
        /* Tset.c writes all ones, not 1. */
        (void)ndbus_pool_write32(pool, offset, 0xFFFFFFFFu);
    }
    ndbus_unlock();

    if (out_previous != NULL)
    {
        *out_previous = previous;
    }
    return taken;
}

bool ndbus_tset16(NdbusPool *pool, uint32_t offset, uint16_t taken_value)
{
    if (!ndbus_pool_contains(pool, offset, 2))
    {
        return false;
    }

    ndbus_lock();
    bool taken = (ndbus_pool_read16(pool, offset) == 0);
    if (taken)
    {
        (void)ndbus_pool_write16(pool, offset, taken_value);
    }
    ndbus_unlock();
    return taken;
}

void ndbus_semaphore_release16(NdbusPool *pool, uint32_t offset)
{
    ndbus_lock();
    (void)ndbus_pool_write16(pool, offset, 0);
    ndbus_unlock();
}

void ndbus_semaphore_release32(NdbusPool *pool, uint32_t offset)
{
    ndbus_lock();
    (void)ndbus_pool_write32(pool, offset, 0);
    ndbus_unlock();
}

void ndbus_spin_damper_reset(NdbusSpinDamper *damper)
{
    if (damper == NULL)
    {
        return;
    }
    damper->last_offset  = 0;
    damper->repeat_count = 0;
    damper->have_last    = false;
}

bool ndbus_spin_damper_failed(NdbusSpinDamper *damper, uint32_t offset, const NdbusHostOps *host)
{
    if (damper == NULL)
    {
        return false;
    }

    /* A different address means the guest is making progress through several
     * cells, not spinning on one. Start the streak over. */
    if (!damper->have_last || damper->last_offset != offset)
    {
        damper->have_last    = true;
        damper->last_offset  = offset;
        damper->repeat_count = 1;
        return false;
    }

    /* Saturate rather than wrap: a very long spin must not fall back below the
     * threshold and stop yielding. */
    if (damper->repeat_count < UINT32_MAX)
    {
        damper->repeat_count++;
    }

    if (damper->repeat_count < NDBUS_SPIN_YIELD_THRESHOLD)
    {
        return false;
    }

    if (host != NULL && host->yield != NULL)
    {
        host->yield(host->ctx);
        return true;
    }
    return false;
}

void ndbus_spin_damper_succeeded(NdbusSpinDamper *damper)
{
    if (damper == NULL)
    {
        return;
    }
    damper->have_last    = false;
    damper->repeat_count = 0;
}
