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
 *     words 6-7  X5FIF   ring base, a 32-bit value, HIGH WORD FIRST
 *
 *   PER-CPU extension block, at slot CPUNO, stride 200B words (128 words,
 *   256 bytes), so block N starts at header + N * 256 bytes:
 *     words 0-1  X5BEX   queue head
 *     word 5     X5ACT   work flag - THE DOORBELL
 *     word 6     X5PRO
 *     word 10B   X5CLR
 *     word 11B   X5CCL
 *
 * X5FIF IS A BYTE OFFSET, NOT A WORD ADDRESS, and it is relative to the window.
 * It uses the same convention as X5BEX and LINK. The microcode settles it:
 * SYS_DATAF at 025636 copies header word 6 into srf[0o2002] with NO shift, and
 * GIVEINT then uses it DIRECTLY as a byte address (slot = ringbase + fill * 4).
 * RetroCore records that an earlier "ring >> 1" word-address form appeared to
 * work only because a hardcoded "<< 1" in the servicer cancelled it; when that
 * shift was removed the real convention showed. Storing a word address here
 * puts every ring slot at half its true offset, which lands inside the
 * structure rather than outside it - so it corrupts rather than faults.
 *
 * XMSINIT'S PICTURE - the state the structure starts in:
 *     X5SEM = 0        the semaphore is FREE
 *     X5HEN = 0
 *     X5FYL = 0
 *     X5MXF = the ring slot count
 *     X5FIF = the ring base, as the byte offset above
 *     X5BEX = -1       empty chain
 *     X5ACT = -1       nothing pending
 *     X5PRO = -1       idle
 * Note that X5SEM's free value is 0 while the three per-CPU cells are -1. A
 * structure zero-filled instead of initialised therefore looks like a CPU with
 * a queued chain at offset 0 and a doorbell already rung.
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
 * RINGS ARE NOT MESSAGES, AND THE DOORBELL COALESCES. X5ACT says "there is work
 * in the queue", not "here is one unit of work". A ring that lands while the CPU
 * is between reading X5ACT and re-arming it is absorbed and never seen as a
 * separate ring - and that is CORRECT, because the microcode's answer to a ring
 * is to walk the X5BEX chain, which finds everything queued. Counting rings and
 * expecting the count to equal the number of messages is wrong on real hardware
 * and wrong here.
 *
 * The consequence for correctness is that the QUEUE is what must not lose work,
 * not the doorbell. A ring may be coalesced; a queued message may not be
 * dropped, and must not be serviced twice.
 *
 * ONE POLLER PER DOORBELL. ndbus_mailbox_poll() reads X5ACT and then re-arms it,
 * and those two accesses are NOT atomic with respect to each other. That is safe
 * because a doorbell belongs to exactly one CPU and only that CPU polls it - the
 * same ownership argument as ndbus_doorbell_take(). A cell that several parties
 * may consume is a semaphore, not a doorbell, and goes through ndbus_tset16().
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

/** X5ACT value XMSINIT writes at initialisation: nothing pending. */
#define NDBUS_MBX_X5ACT_INIT 0xFFFFu

/** X5BEX value XMSINIT writes: an empty chain. */
#define NDBUS_MBX_X5BEX_INIT 0xFFFFu

/** X5PRO value XMSINIT writes: idle. */
#define NDBUS_MBX_X5PRO_INIT 0xFFFFu

/** X5SEM's free value. NOT -1 - see XMSINIT's picture above. */
#define NDBUS_MBX_X5SEM_FREE 0u

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

/**
 * @brief Seed the structure the way XMSINIT does.
 *
 * Writes the global header and THIS CPU's extension block to the values listed
 * under "XMSINIT's picture" above. It does not touch any other CPU's block, so
 * bringing a second CPU up does not disturb a running first one.
 *
 * @param mbx        The mailbox view.
 * @param ring_slots The ring slot count, stored in X5MXF.
 * @param ring_byte  Ring base as a WINDOW-RELATIVE BYTE offset, stored in X5FIF
 *                   high word first. A word address here puts every slot at
 *                   half its true offset.
 * @return true on success; false when unattached or the writes do not fit.
 */
bool ndbus_mailbox_init_xmsinit(NdbusMailbox *mbx, uint16_t ring_slots, uint32_t ring_byte);

#endif /* NDBUS_MAILBOX_H */
