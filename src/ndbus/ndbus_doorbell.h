/**
 * @file ndbus_doorbell.h
 * @brief The one place ordering is required.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * A CPU fills a message in the pool and then rings a doorbell. The reader must
 * not see the doorbell before the message.
 *
 *   writer: message bytes RELAXED, then the doorbell cell RELEASE
 *   reader: the doorbell cell ACQUIRE, then the message bytes RELAXED
 *
 * On x86-64's TSO model the relaxed version happens to work. DO NOT RELY ON IT.
 * AArch64 and the browser will reorder it, and the failure mode is an
 * occasionally half-written message, which presents as "the ND-5000 hung" with
 * nothing in any log.
 *
 * This is the ONLY place in ndbus that uses anything stronger than relaxed. The
 * bytes of the message itself are ordinary pool accesses and stay relaxed - the
 * release on the doorbell store is what publishes them.
 */

#ifndef NDBUS_DOORBELL_H
#define NDBUS_DOORBELL_H

#include "ndbus_pool.h"

/**
 * @brief Ring: store `value` to the doorbell cell with RELEASE ordering,
 *        publishing every relaxed write the calling thread made to the pool
 *        before it.
 *
 * @param pool   The shared pool holding the doorbell cell.
 * @param offset Byte offset of the doorbell cell within the pool.
 * @param value  The flag byte to store.
 * @return true when the byte was stored; false when the cell is outside the
 *         pool, in which case nothing is stored.
 *
 * @note The doorbell is one BYTE. It carries a flag, never data - the message
 *       is in the pool. A byte cannot tear on any host, so no wider form is
 *       needed and none is offered.
 */
bool ndbus_doorbell_ring(NdbusPool *pool, uint32_t offset, uint8_t value);

/**
 * @brief Read the doorbell cell with ACQUIRE ordering.
 *
 * Every pool byte the ringer wrote before ringing is visible to this thread
 * afterwards.
 *
 * @param pool   The shared pool holding the doorbell cell.
 * @param offset Byte offset of the doorbell cell within the pool.
 * @return The doorbell byte; 0 for a cell outside the pool, which is the same
 *         as "not rung" - a caller that needs to tell the two apart checks
 *         ndbus_pool_contains() itself.
 */
uint8_t ndbus_doorbell_read(const NdbusPool *pool, uint32_t offset);

/**
 * @brief Read the doorbell with acquire ordering and, if it was non-zero,
 *        clear it - the whole take-the-message step.
 *
 * @param pool   The shared pool holding the doorbell cell.
 * @param offset Byte offset of the doorbell cell within the pool.
 * @return The value that was there; 0 if it was already clear or the cell is
 *         outside the pool.
 *
 * @note NOT a read-modify-write under the bus mutex: a doorbell has exactly one
 *       reader (the CPU it belongs to), so there is no second party to race
 *       with. A cell that several CPUs may clear is a semaphore, not a doorbell
 *       - use ndbus_tset16.
 */
uint8_t ndbus_doorbell_take(NdbusPool *pool, uint32_t offset);

#endif /* NDBUS_DOORBELL_H */
