/**
 * @file ndbus_msgqueue.h
 * @brief The X5BEX message chain: the queue a doorbell tells a CPU to walk.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * The doorbell says "there is work"; THIS is the work. X5BEX in a CPU's
 * extension block is the head of a singly-linked chain of message blocks in
 * shared memory, each one carrying the offset of the next in its LINK field.
 * Because the answer to a ring is to walk the whole chain, a coalesced ring
 * loses nothing - see ndbus_mailbox.h.
 *
 * LINKS ARE WINDOW-RELATIVE BYTE OFFSETS on the octobus generation. RetroCore
 * records the transport seam plainly: "octobus = byte identity, ND500 = <<1",
 * and applying the ND-500 word shift on the octobus path put a slot at twice the
 * address the microcode wrote. The same convention X5FIF uses. We only build the
 * octobus generation, so links are byte offsets throughout and no shift exists
 * anywhere in this file - if one ever appears, it is a bug.
 *
 * -1 TERMINATES, and it is also what an empty X5BEX holds. Zero does NOT: offset
 * 0 is a legal place for a message block, so a chain terminated with 0 would
 * walk into the mailbox header.
 *
 * MESSAGE BLOCK FIELDS, as WORD offsets (byte = word * 2). From the microcode
 * decode and the byte-verified SWMSG field dossier, via RetroCore's
 * N5MessageOffsets:
 *
 *     0-1   LINK    next block, or -1
 *     2     N5STA   message status
 *     3     SENDE   sender; the watchdog uses -1
 *     4     X5CPU   receiver / CPU field
 *     5     X5ACT   size / activation field
 *     6     MICFU   micro-function
 *     7     N500A / SWFUN   OVERLAY - see the warning below
 *     10B   N500A_LO / SWRST OVERLAY - see the warning below
 *     11B   STOPR   stop reason / N100A / ACPRO, direction-dependent
 *     12B   NUMPA   number of parameters
 *     13B   MCNO    mon-call number / NRBYT byte count, direction-dependent
 *     14B   MSWMC   swapper mon-call field
 *     16B   TRAPN   trap number
 *     37B   SMCNO   saved mon-call number
 *     101B  SWPFU   swapper function code
 *     103B  SWPST   swapper status
 *
 * THE OVERLAY AT 7 AND 10B IS A TRAP, and RetroCore records that it cost a full
 * day. Words 7 and 10B look like the high and low halves of one 32-bit ND-500
 * address, and reading them that way is correct ONLY for the RESIRD/RESIWR
 * block-copy arm. In the swapper direction 10B is its own field, SWRST, and the
 * real swapper reads it as a SEGMENT NUMBER and range-checks it. A reader that
 * always treats 7-10B as one address gets a plausible wrong answer on every
 * swapper request. Which arm applies is decided by MICFU, so the two are named
 * separately here and no helper reads them as a pair.
 */

#ifndef NDBUS_MSGQUEUE_H
#define NDBUS_MSGQUEUE_H

#include "ndbus_mailbox.h"

/** A link value meaning "no next block", and an empty X5BEX. NOT zero. */
#define NDBUS_MSG_LINK_NONE 0xFFFFFFFFu

/* ---- message block field WORD offsets ----------------------------------- */
#define NDBUS_MSG_LINK_WORD  0u  /**< words 0-1, 32-bit byte offset of the next block */
#define NDBUS_MSG_N5STA      2u  /**< message status */
#define NDBUS_MSG_SENDE      3u  /**< sender; watchdog is -1 */
#define NDBUS_MSG_X5CPU      4u  /**< receiver / CPU */
#define NDBUS_MSG_X5ACT      5u  /**< size / activation */
#define NDBUS_MSG_MICFU      6u  /**< micro-function; decides the 7/10B overlay arm */
#define NDBUS_MSG_N500A      7u  /**< ND-500 address HIGH half, or SWFUN. OVERLAY */
#define NDBUS_MSG_SWRST      8u  /**< 10B: address LOW half, or SWRST. OVERLAY */
#define NDBUS_MSG_STOPR      9u  /**< 11B */
#define NDBUS_MSG_NUMPA     10u  /**< 12B */
#define NDBUS_MSG_MCNO      11u  /**< 13B */
#define NDBUS_MSG_MSWMC     12u  /**< 14B */
#define NDBUS_MSG_TRAPN     14u  /**< 16B */
#define NDBUS_MSG_SMCNO     31u  /**< 37B */
#define NDBUS_MSG_SWPFU     65u  /**< 101B */
#define NDBUS_MSG_SWPST     67u  /**< 103B */

/**
 * @brief Read a message block's LINK: the byte offset of the next block.
 * @param pool      The shared pool.
 * @param msg_byte  Pool byte offset of the block.
 * @return The next block's byte offset, or NDBUS_MSG_LINK_NONE at the end.
 */
uint32_t ndbus_msg_link(const NdbusPool *pool, uint32_t msg_byte);

/**
 * @brief Set a message block's LINK.
 * @param pool     The shared pool.
 * @param msg_byte Pool byte offset of the block.
 * @param next     Byte offset of the next block, or NDBUS_MSG_LINK_NONE.
 * @return true on success; false when the block is outside the pool.
 */
bool ndbus_msg_set_link(NdbusPool *pool, uint32_t msg_byte, uint32_t next);

/**
 * @brief Read one 16-bit field of a message block.
 * @param pool      The shared pool.
 * @param msg_byte  Pool byte offset of the block.
 * @param word      An NDBUS_MSG_* word offset.
 * @return The field, or 0 when outside the pool.
 */
uint16_t ndbus_msg_read(const NdbusPool *pool, uint32_t msg_byte, uint32_t word);

/**
 * @brief Write one 16-bit field of a message block.
 * @param pool      The shared pool.
 * @param msg_byte  Pool byte offset of the block.
 * @param word      An NDBUS_MSG_* word offset.
 * @param value     The value.
 * @return true on success; false when outside the pool.
 */
bool ndbus_msg_write(NdbusPool *pool, uint32_t msg_byte, uint32_t word, uint16_t value);

/**
 * @brief Post a message block onto this CPU's X5BEX chain.
 *
 * Takes the bus lock, so it is mutually exclusive with a concurrent take and
 * with a guest TSET on X5SEM. The block is linked at the HEAD, which is O(1) and
 * is why the chain needs no tail pointer.
 *
 * @param mbx      The receiving CPU's mailbox view.
 * @param msg_byte Pool byte offset of the block to post.
 * @return true on success; false when unattached or the block is outside the
 *         pool.
 */
bool ndbus_msgqueue_post(NdbusMailbox *mbx, uint32_t msg_byte);

/**
 * @brief Take the next message block off this CPU's X5BEX chain.
 *
 * Takes the bus lock. Returns blocks in the reverse of the order they were
 * posted, because posting is at the head - which matters only to a test, since
 * the microcode walks the chain rather than relying on an order.
 *
 * @param mbx           The CPU's mailbox view.
 * @param out_msg_byte  Receives the block's byte offset. Untouched when empty.
 * @return true when a block was taken; false when the chain is empty.
 */
bool ndbus_msgqueue_take(NdbusMailbox *mbx, uint32_t *out_msg_byte);

/**
 * @brief How many blocks are on this CPU's chain.
 *
 * Walks the chain under the bus lock, and stops at @p max_walk links to bound a
 * chain that a bug has made circular - a cycle would otherwise hang the caller
 * with no indication of why.
 *
 * @param mbx      The CPU's mailbox view.
 * @param max_walk Largest number of links to follow.
 * @return The count, or max_walk when the chain is longer or circular.
 */
int ndbus_msgqueue_depth(const NdbusMailbox *mbx, int max_walk);

#endif /* NDBUS_MSGQUEUE_H */
