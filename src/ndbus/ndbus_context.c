/**
 * @file ndbus_context.c
 * @brief The SAMSON context block.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include <string.h>

#include "ndbus_context.h"

static bool attached(const NdbusContext *ctx)
{
    return ctx != NULL && ctx->pool != NULL && ctx->x5cpu >= 0;
}

bool ndbus_context_attach(NdbusContext *ctx, NdbusPool *pool, uint32_t area_byte, int x5cpu)
{
    if (ctx == NULL)
    {
        return false;
    }
    memset(ctx, 0, sizeof(*ctx));

    if (pool == NULL || x5cpu < 0 || x5cpu > NDBUS_CTX_MAX_CPU)
    {
        return false;
    }

    /* The block is one stride past the area base, plus this CPU's stride. */
    uint64_t base = (uint64_t)area_byte + NDBUS_CTX_STRIDE_BYTES +
                    ((uint64_t)x5cpu * NDBUS_CTX_STRIDE_BYTES);
    if (base + NDBUS_CTX_STRIDE_BYTES > (uint64_t)pool->size)
    {
        /* Refused rather than clamped: a block that runs off the end reads as
         * zeros, and a context of all zeros is a CPU started at P = 0. */
        return false;
    }

    ctx->pool = pool;
    ctx->area_byte = area_byte;
    ctx->x5cpu = x5cpu;
    return true;
}

uint32_t ndbus_context_base(const NdbusContext *ctx)
{
    if (!attached(ctx))
    {
        return 0;
    }
    return ctx->area_byte + NDBUS_CTX_STRIDE_BYTES +
           ((uint32_t)ctx->x5cpu * NDBUS_CTX_STRIDE_BYTES);
}

uint32_t ndbus_context_read(const NdbusContext *ctx, uint32_t field)
{
    if (!attached(ctx) || field >= NDBUS_CTX_STRIDE_BYTES)
    {
        return 0;
    }
    return ndbus_pool_read32(ctx->pool, ndbus_context_base(ctx) + field);
}

bool ndbus_context_write(NdbusContext *ctx, uint32_t field, uint32_t value)
{
    if (!attached(ctx) || field >= NDBUS_CTX_STRIDE_BYTES)
    {
        return false;
    }
    return ndbus_pool_write32(ctx->pool, ndbus_context_base(ctx) + field, value);
}

bool ndbus_context_field_is_loaded(uint32_t field)
{
    switch (field)
    {
    /* DOMAIN registers: sourced from the Domain Information Table, NOT from the
     * block. NEWCNTXT does not touch them however they are filled in. */
    case NDBUS_CTX_DIT_TOS:
    case NDBUS_CTX_DIT_LL:
    case NDBUS_CTX_DIT_HL:
    case NDBUS_CTX_DIT_THA:
    case NDBUS_CTX_DIT_CES:
    case NDBUS_CTX_DIT_CAS:
    case NDBUS_CTX_DIT_OTE1:
    case NDBUS_CTX_DIT_OTE2:
    case NDBUS_CTX_DIT_CTE1:
    case NDBUS_CTX_DIT_CTE2:
    case NDBUS_CTX_DIT_MTE1:
    case NDBUS_CTX_DIT_MTE2:
    case NDBUS_CTX_DIT_TEM1:
    case NDBUS_CTX_DIT_TEM2:
        return false;
    default:
        break;
    }

    /* Everything else inside the block is loaded. Past the decoded area is not
     * a field at all. */
    return field <= NDBUS_CTX_SC2;
}

bool ndbus_context_place(NdbusContext *ctx, uint32_t entry_p, uint32_t local_base)
{
    if (!attached(ctx))
    {
        return false;
    }

    uint32_t base = ndbus_context_base(ctx);
    if (!ndbus_pool_contains(ctx->pool, base, NDBUS_CTX_STRIDE_BYTES))
    {
        return false;
    }

    /* Clear the whole block first. A block left from a previous run would
     * otherwise leak its register file into this one, and a stale B or CED is a
     * fault a long way from its cause. */
    for (uint32_t off = 0; off < NDBUS_CTX_STRIDE_BYTES; off += 4u)
    {
        (void)ndbus_pool_write32(ctx->pool, base + off, 0u);
    }

    bool ok = true;
    ok = ok && ndbus_context_write(ctx, NDBUS_CTX_P, entry_p);
    ok = ok && ndbus_context_write(ctx, NDBUS_CTX_B, local_base);
    return ok;
}
