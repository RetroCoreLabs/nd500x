/**
 * @file ndbus_msgqueue.c
 * @brief The X5BEX message chain.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "ndbus_lock.h"
#include "ndbus_msgqueue.h"

/* A block's LINK is a 32-bit value in its first two words. */
static uint32_t link_byte(uint32_t msg_byte)
{
    return msg_byte + (NDBUS_MSG_LINK_WORD * 2u);
}

uint32_t ndbus_msg_link(const NdbusPool *pool, uint32_t msg_byte)
{
    if (!ndbus_pool_contains(pool, link_byte(msg_byte), 4))
    {
        return NDBUS_MSG_LINK_NONE;
    }
    return ndbus_pool_read32(pool, link_byte(msg_byte));
}

bool ndbus_msg_set_link(NdbusPool *pool, uint32_t msg_byte, uint32_t next)
{
    if (!ndbus_pool_contains(pool, link_byte(msg_byte), 4))
    {
        return false;
    }
    return ndbus_pool_write32(pool, link_byte(msg_byte), next);
}

uint16_t ndbus_msg_read(const NdbusPool *pool, uint32_t msg_byte, uint32_t word)
{
    return ndbus_pool_read16(pool, msg_byte + (word * 2u));
}

bool ndbus_msg_write(NdbusPool *pool, uint32_t msg_byte, uint32_t word, uint16_t value)
{
    return ndbus_pool_write16(pool, msg_byte + (word * 2u), value);
}

/* X5BEX is 32 bits in the extension block's first two words. The mailbox helpers
 * are 16-bit, so the head is assembled here - high word first, as everywhere. */
static uint32_t read_head(const NdbusMailbox *mbx)
{
    uint32_t hi = ndbus_mailbox_read_ext(mbx, NDBUS_MBX_X5BEX_WORD);
    uint32_t lo = ndbus_mailbox_read_ext(mbx, NDBUS_MBX_X5BEX_WORD + 1u);
    return (hi << 16u) | lo;
}

static bool write_head(NdbusMailbox *mbx, uint32_t value)
{
    bool ok = ndbus_mailbox_write_ext(mbx, NDBUS_MBX_X5BEX_WORD, (uint16_t)(value >> 16u));
    ok = ok && ndbus_mailbox_write_ext(mbx, NDBUS_MBX_X5BEX_WORD + 1u,
                                       (uint16_t)(value & 0xFFFFu));
    return ok;
}

bool ndbus_msgqueue_post(NdbusMailbox *mbx, uint32_t msg_byte)
{
    if (mbx == NULL || mbx->pool == NULL)
    {
        return false;
    }
    if (!ndbus_pool_contains(mbx->pool, link_byte(msg_byte), 4))
    {
        return false;
    }

    /* Under the bus lock, so a concurrent take cannot read the head between the
     * two writes below and walk into a chain that is half-relinked. */
    ndbus_lock();
    uint32_t head = read_head(mbx);
    bool ok = ndbus_msg_set_link(mbx->pool, msg_byte, head);
    ok = ok && write_head(mbx, msg_byte);
    ndbus_unlock();
    return ok;
}

bool ndbus_msgqueue_take(NdbusMailbox *mbx, uint32_t *out_msg_byte)
{
    if (mbx == NULL || mbx->pool == NULL)
    {
        return false;
    }

    ndbus_lock();
    uint32_t head = read_head(mbx);
    bool have = (head != NDBUS_MSG_LINK_NONE);
    if (have)
    {
        /* The taken block's LINK becomes the new head. Its own link is left as
         * it was: the caller owns the block now, and clearing it here would
         * destroy a field the caller may want to read. */
        uint32_t next = ndbus_msg_link(mbx->pool, head);
        have = write_head(mbx, next);
        if (have && out_msg_byte != NULL)
        {
            *out_msg_byte = head;
        }
    }
    ndbus_unlock();
    return have;
}

int ndbus_msgqueue_depth(const NdbusMailbox *mbx, int max_walk)
{
    if (mbx == NULL || mbx->pool == NULL || max_walk <= 0)
    {
        return 0;
    }

    ndbus_lock();
    int      n = 0;
    uint32_t at = read_head(mbx);
    while (at != NDBUS_MSG_LINK_NONE && n < max_walk)
    {
        n++;
        at = ndbus_msg_link(mbx->pool, at);
    }
    ndbus_unlock();
    /* Hitting max_walk means the chain is longer than asked for OR circular. The
     * caller cannot tell the two apart, which is deliberate: both mean "do not
     * trust this count", and a walk that looped forever would hang with nothing
     * to show for it. */
    return n;
}
