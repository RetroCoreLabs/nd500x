/**
 * @file ndbus_mailbox.c
 * @brief The ND-100 / ND-5000 mailbox in MPM-5 shared memory.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include <string.h>

#include "ndbus_doorbell.h"
#include "ndbus_lock.h"
#include "ndbus_mailbox.h"

/** Byte offset of a word in the global header. */
static uint32_t global_byte(const NdbusMailbox *mbx, uint32_t word_index)
{
    return mbx->header_byte + (word_index * 2u);
}

/** Byte offset of a word in this CPU's extension block. */
static uint32_t ext_byte(const NdbusMailbox *mbx, uint32_t word_index)
{
    return ndbus_mailbox_ext_base(mbx) + (word_index * 2u);
}

static bool attached(const NdbusMailbox *mbx)
{
    return mbx != NULL && mbx->pool != NULL && mbx->cpuno >= 1;
}

bool ndbus_mailbox_attach(NdbusMailbox *mbx, NdbusPool *pool, uint32_t header_byte, int cpuno)
{
    if (mbx == NULL)
    {
        return false;
    }
    memset(mbx, 0, sizeof(*mbx));

    if (pool == NULL)
    {
        return false;
    }

    /* CPUNO IS 1-BASED. Slot 0 is the global header, so a CPUNO of 0 would put
     * this CPU's extension block on top of it and overwrite X5SEM with a queue
     * pointer - a corruption that looks like a stuck semaphore. */
    if (cpuno < 1 || cpuno > NDBUS_MBX_MAX_CPUNO)
    {
        return false;
    }

    /* The header plus this CPU's whole extension block must fit. Refused rather
     * than clamped: a mailbox that runs off the end of the pool reads as zeros,
     * which is indistinguishable from an empty queue. */
    uint64_t end = (uint64_t)header_byte + ((uint64_t)(cpuno + 1) * NDBUS_MBX_STRIDE_BYTES);
    if (end > (uint64_t)pool->size)
    {
        return false;
    }

    mbx->pool = pool;
    mbx->header_byte = header_byte;
    mbx->cpuno = cpuno;
    return true;
}

uint32_t ndbus_mailbox_ext_base(const NdbusMailbox *mbx)
{
    if (!attached(mbx))
    {
        return 0;
    }
    return mbx->header_byte + ((uint32_t)mbx->cpuno * NDBUS_MBX_STRIDE_BYTES);
}

uint16_t ndbus_mailbox_read_global(const NdbusMailbox *mbx, uint32_t word_index)
{
    if (!attached(mbx))
    {
        return 0;
    }
    return ndbus_pool_read16(mbx->pool, global_byte(mbx, word_index));
}

bool ndbus_mailbox_write_global(NdbusMailbox *mbx, uint32_t word_index, uint16_t value)
{
    if (!attached(mbx))
    {
        return false;
    }
    return ndbus_pool_write16(mbx->pool, global_byte(mbx, word_index), value);
}

uint16_t ndbus_mailbox_read_ext(const NdbusMailbox *mbx, uint32_t word_index)
{
    if (!attached(mbx))
    {
        return 0;
    }
    return ndbus_pool_read16(mbx->pool, ext_byte(mbx, word_index));
}

bool ndbus_mailbox_write_ext(NdbusMailbox *mbx, uint32_t word_index, uint16_t value)
{
    if (!attached(mbx))
    {
        return false;
    }
    return ndbus_pool_write16(mbx->pool, ext_byte(mbx, word_index), value);
}

bool ndbus_mailbox_init_xmsinit(NdbusMailbox *mbx, uint16_t ring_slots, uint32_t ring_byte)
{
    if (!attached(mbx))
    {
        return false;
    }

    bool ok = true;
    /* X5SEM free is 0, not -1. A zero-filled structure would look free here but
     * would also look like a queued chain at offset 0 with the doorbell rung. */
    ok = ok && ndbus_mailbox_write_global(mbx, NDBUS_MBX_X5SEM_WORD, NDBUS_MBX_X5SEM_FREE);
    ok = ok && ndbus_mailbox_write_global(mbx, NDBUS_MBX_X5HEN_WORD, 0u);
    ok = ok && ndbus_mailbox_write_global(mbx, NDBUS_MBX_X5FYL_WORD, 0u);
    ok = ok && ndbus_mailbox_write_global(mbx, NDBUS_MBX_X5MXF_WORD, ring_slots);

    /* X5FIF is a 32-bit BYTE offset, high word first. Not a word address: the
     * microcode uses it directly as a byte address. */
    ok = ok && ndbus_mailbox_write_global(mbx, NDBUS_MBX_X5FIF_WORD,
                                          (uint16_t)(ring_byte >> 16u));
    ok = ok && ndbus_mailbox_write_global(mbx, NDBUS_MBX_X5FIF_WORD + 1u,
                                          (uint16_t)(ring_byte & 0xFFFFu));

    /* This CPU's block only - a second CPU coming up must not disturb a running
     * first one. X5BEX is 32 bits, so both its words are set. */
    ok = ok && ndbus_mailbox_write_ext(mbx, NDBUS_MBX_X5BEX_WORD, NDBUS_MBX_X5BEX_INIT);
    ok = ok && ndbus_mailbox_write_ext(mbx, NDBUS_MBX_X5BEX_WORD + 1u, NDBUS_MBX_X5BEX_INIT);
    ok = ok && ndbus_mailbox_write_ext(mbx, NDBUS_MBX_X5ACT_WORD, NDBUS_MBX_X5ACT_INIT);
    ok = ok && ndbus_mailbox_write_ext(mbx, NDBUS_MBX_X5PRO_WORD, NDBUS_MBX_X5PRO_INIT);
    return ok;
}

bool ndbus_mailbox_take_sem(NdbusMailbox *mbx, uint16_t taken_value)
{
    if (!attached(mbx))
    {
        return false;
    }
    /* Through ndbus_tset16, so this shares the one bus-wide lock domain with
     * every other LOCK cycle - including a guest TSET on this same cell. */
    return ndbus_tset16(mbx->pool, global_byte(mbx, NDBUS_MBX_X5SEM_WORD), taken_value);
}

void ndbus_mailbox_release_sem(NdbusMailbox *mbx)
{
    if (!attached(mbx))
    {
        return;
    }
    ndbus_semaphore_release16(mbx->pool, global_byte(mbx, NDBUS_MBX_X5SEM_WORD));
}

bool ndbus_mailbox_ring(NdbusMailbox *mbx)
{
    if (!attached(mbx))
    {
        return false;
    }

    /*
     * ACT51 writes X5ACT := 0, and sends NO kick. The kick is the preempt path.
     *
     * X5ACT is a 16-bit word and the doorbell helper works on bytes, so the
     * release goes on the byte the reader's acquire will load - the HIGH byte,
     * which is the first of the big-endian pair - and the low byte is written
     * first as an ordinary store. Everything written before the release is
     * published by it.
     */
    uint32_t act = ext_byte(mbx, NDBUS_MBX_X5ACT_WORD);
    if (!ndbus_pool_contains(mbx->pool, act, 2))
    {
        return false;
    }
    (void)ndbus_pool_write8(mbx->pool, act + 1u, 0u);
    return ndbus_doorbell_ring(mbx->pool, act, 0u);
}

bool ndbus_mailbox_poll(NdbusMailbox *mbx)
{
    if (!attached(mbx))
    {
        return false;
    }

    uint32_t act = ext_byte(mbx, NDBUS_MBX_X5ACT_WORD);
    if (!ndbus_pool_contains(mbx->pool, act, 2))
    {
        return false;
    }

    /* Acquire on the high byte pairs with the release in _ring(), so the message
     * the ND-100 wrote before ringing is visible to this CPU afterwards. */
    uint8_t  high = ndbus_doorbell_read(mbx->pool, act);
    uint16_t value = (uint16_t)(((uint16_t)high << 8u) |
                                (uint16_t)ndbus_pool_read8(mbx->pool, act + 1u));
    if (value != 0u)
    {
        return false; /* not rung */
    }

    /* RE-ARM WITH 1, NOT -1. This is what makes the 0xFFFF to 0 signature occur
     * exactly once per XMSINIT, and therefore why a self-discovery sniff with a
     * repeat threshold above 1 can never latch. */
    (void)ndbus_pool_write16(mbx->pool, act, (uint16_t)NDBUS_MBX_X5ACT_REARM);
    return true;
}
