/**
 * @file ndbus_mailbox.h
 * @brief The ND-100 / ND-5000 mailbox in MPM-5 shared memory.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * THE OCTOBUS CARRIES NO DATA. ND-05.020.01 chapter 5.3: the bus is for
 * messages that INITIATE operations, and "as a general rule, these operations
 * work on data in shared memory in the MFbus system". So the whole mailbox lives
 * in the MPM-5 pool, and the octobus only ever says "go look".
 *
 * LAYOUT. Taken from the NPL source and the microcode, via RetroCore's
 * OctobusND5000Station (its citation: NPL-V + microcode V, architecture doc
 * section 7.2). Everything is 16-bit WORDS, big-endian in the pool, and the word
 * numbers below are the ones the ND documents use - so the octal ones are
 * written octal.
 *
 *   GLOBAL header, at stride slot 0:
 *     word 0     X5SEM   the semaphore guarding the whole structure
 *     word 3     X5HEN
 *     word 4     X5FYL
 *     word 5     X5MXF
 *     words 6-7  X5FIF   ring base
 *
 *   PER-CPU extension block, at slot CPUNO, stride 200B words (128 words,
 *   256 bytes), so block N starts at header + N * 256 bytes:
 *     words 0-1  X5BEX   queue head
 *     word 5     X5ACT   work flag - THE DOORBELL
 *     word 6     X5PRO
 *     word 10B   X5CLR
 *     word 11B   X5CCL
 *
 * CPUNO IS 1-BASED. Slot 0 is the global header, so the first ND-5000 is CPUNO
 * 1 at header + 256. Stations 070B..073B are CPUNO 1..4. Treating CPUNO as
 * 0-based would put the first CPU's extension block on top of the global header
 * and corrupt the semaphore with a queue pointer.
 *
 * THE DOORBELL PROTOCOL, and it is not symmetric:
 *
 *   - SINTRAN's ACT51 writes X5ACT := 0. That is the idle wakeup, and it sends
 *     NO kick.
 *   - The microcode's IDLE loop POLLS X5ACT, re-arms it to 1, and walks the
 *     X5BEX chain.
 *   - A kick (N100KICK = 1) is the PREEMPT path only, not the normal one.
 *
 * This is why the self-discovery sniff in ndbus_nd5000.h can only ever see ONE
 * 0xFFFF to 0 transition per XMSINIT: XMSINIT initialises X5ACT to -1, and every
 * re-arm after that writes 1, so every later ring is 1 to 0. The two facts are
 * the same fact seen from two sides.
 *
 * LOCKING. X5SEM is a semaphore in shared memory, so it goes through
 * ndbus_tset16() and therefore through the one bus-wide mutex that every LOCK
 * cycle uses. Anything that must be mutually exclusive with it - the activation
 * critical sections - takes the same lock. RetroCore records the audit finding
 * behind that in one line: a lock on one side of the race is no lock at all.
 */

#ifndef NDBUS_MAILBOX_H
#define NDBUS_MAILBOX_H

#include "ndbus_pool.h"

/** Stride between the global header and each per-CPU extension block, in bytes.
 *  200B words = 128 words = 256 bytes. */
#define NDBUS_MBX_STRIDE_BYTES 256u

/** Highest CPUNO the structure addresses. CPUNO is 1-based; slot 0 is global. */
#define NDBUS_MBX_MAX_CPUNO 7

/* ---- global header word offsets (words, not bytes) ---------------------- */
#define NDBUS_MBX_X5SEM_WORD 0u  /**< semaphore guarding the structure */
#define NDBUS_MBX_X5HEN_WORD 3u
#define NDBUS_MBX_X5FYL_WORD 4u
#define NDBUS_MBX_X5MXF_WORD 5u
#define NDBUS_MBX_X5FIF_WORD 6u  /**< ring base, words 6-7 */

/* ---- per-CPU extension block word offsets ------------------------------- */
#define NDBUS_MBX_X5BEX_WORD 0u  /**< queue head, words 0-1 */
#define NDBUS_MBX_X5ACT_WORD 5u  /**< work flag - the doorbell. Byte 0x0A */
#define NDBUS_MBX_X5PRO_WORD 6u
#define NDBUS_MBX_X5CLR_WORD 8u  /**< word 10B */
#define NDBUS_MBX_X5CCL_WORD 9u  /**< word 11B */

/** X5ACT value written by the microcode when it re-arms the doorbell. NOT -1:
 *  see the doorbell protocol above, and the sniff trap in ndbus_nd5000.h. */
#define NDBUS_MBX_X5ACT_REARM 1u

/** X5ACT value XMSINIT writes at initialisation. */
#define NDBUS_MBX_X5ACT_INIT 0xFFFFu

/** One machine's mailbox: where it sits in the pool, and which CPU we are. */
typedef struct NdbusMailbox
{
    NdbusPool *pool;        /**< the shared MPM-5 pool the mailbox lives in */
    uint32_t   header_byte; /**< pool BYTE offset of the global header */
    int        cpuno;       /**< this CPU's CPUNO, 1-based */
} NdbusMailbox;

/**
 * @brief Point a mailbox view at the global header in the pool.
 *
 * @param mbx         The view to fill.
 * @param pool        The shared pool.
 * @param header_byte Pool BYTE offset of the global header.
 * @param cpuno       This CPU's CPUNO, 1-based, 1..NDBUS_MBX_MAX_CPUNO.
 * @return true on success; false having configured NOTHING when the pool is
 *         missing, when cpuno is outside 1..7 - a 0 would put this CPU's
 *         extension block on top of the global header - or when the header plus
 *         this CPU's extension block does not fit inside the pool.
 */
bool ndbus_mailbox_attach(NdbusMailbox *mbx, NdbusPool *pool, uint32_t header_byte, int cpuno);

/**
 * @brief Byte offset of this CPU's extension block: header + CPUNO * 256.
 * @param mbx The mailbox view.
 * @return The pool byte offset, or 0 when the view is not attached.
 */
uint32_t ndbus_mailbox_ext_base(const NdbusMailbox *mbx);

/**
 * @brief Read one word of the global header.
 * @param mbx        The mailbox view.
 * @param word_index A NDBUS_MBX_*_WORD global offset.
 * @return The word, or 0 when unattached or outside the pool.
 */
uint16_t ndbus_mailbox_read_global(const NdbusMailbox *mbx, uint32_t word_index);

/**
 * @brief Write one word of the global header.
 * @param mbx        The mailbox view.
 * @param word_index A NDBUS_MBX_*_WORD global offset.
 * @param value      The word to store.
 * @return true on success; false when unattached or outside the pool.
 */
bool ndbus_mailbox_write_global(NdbusMailbox *mbx, uint32_t word_index, uint16_t value);

/**
 * @brief Read one word of this CPU's extension block.
 * @param mbx        The mailbox view.
 * @param word_index A per-CPU NDBUS_MBX_*_WORD offset.
 * @return The word, or 0 when unattached or outside the pool.
 */
uint16_t ndbus_mailbox_read_ext(const NdbusMailbox *mbx, uint32_t word_index);

/**
 * @brief Write one word of this CPU's extension block.
 * @param mbx        The mailbox view.
 * @param word_index A per-CPU NDBUS_MBX_*_WORD offset.
 * @param value      The word to store.
 * @return true on success; false when unattached or outside the pool.
 */
bool ndbus_mailbox_write_ext(NdbusMailbox *mbx, uint32_t word_index, uint16_t value);

/**
 * @brief Take X5SEM, the semaphore guarding the mailbox structure.
 *
 * Goes through ndbus_tset16() and therefore through the same bus-wide mutex as
 * every other LOCK cycle, so it is mutually exclusive with a guest TSET on the
 * same cell.
 *
 * @param mbx         The mailbox view.
 * @param taken_value Value stored on success; the caller chooses it because the
 *                    users differ (a host-internal marker, or a real value).
 * @return true when X5SEM was zero and is now held.
 */
bool ndbus_mailbox_take_sem(NdbusMailbox *mbx, uint16_t taken_value);

/**
 * @brief Release X5SEM.
 * @param mbx The mailbox view.
 */
void ndbus_mailbox_release_sem(NdbusMailbox *mbx);

/**
 * @brief Ring this CPU's doorbell the way SINTRAN's ACT51 does: X5ACT := 0.
 *
 * The idle wakeup, which sends NO kick. Release ordering, so every message byte
 * written before it is visible to the CPU that takes it.
 *
 * @param mbx The mailbox view.
 * @return true on success; false when unattached or outside the pool.
 */
bool ndbus_mailbox_ring(NdbusMailbox *mbx);

/**
 * @brief One microcode IDLE-loop poll: read X5ACT and, if rung, re-arm it to 1.
 *
 * Re-arms with 1, NOT -1. That is what makes the 0xFFFF to 0 signature happen
 * exactly once per XMSINIT, and it is the whole reason a self-discovery sniff
 * with a repeat threshold above 1 never latches.
 *
 * @param mbx The mailbox view.
 * @return true when the doorbell had been rung and was consumed by this call.
 */
bool ndbus_mailbox_poll(NdbusMailbox *mbx);

#endif /* NDBUS_MAILBOX_H */
