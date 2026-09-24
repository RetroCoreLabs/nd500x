/**
 * @file ndbus_lock.h
 * @brief The ONE mutex, and the TSET cycle that is the only reason it exists.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * ONE MUTEX FOR ALL LOCK CYCLES - not one per address, one globally.
 *
 * Faithful: the real bank serializes every LOCK cycle at one point too.
 * ND-05.020.01 T100: "In a test-and-set cycle, the memory is locked between the
 * read and write cycles to guarantee that only one process reserves a free
 * semaphore."
 *
 * Correct regardless of alignment and byte order: the obvious alternative,
 * __atomic_exchange_n on a 32-bit view of the pool, requires the guest's operand
 * to be naturally aligned (it need not be - the ND-500 is byte-addressed) and
 * requires the expected and desired values byte-swapped, because the pool holds
 * big-endian bytes on a little-endian host. More code, more ways to be wrong, no
 * measurable gain.
 *
 * Cheap: TSET is rare next to ordinary accesses, and an uncontended mutex is
 * tens of nanoseconds.
 *
 * THE MAILBOX AND DOORBELL OPERATIONS TAKE THIS SAME MUTEX, because they must be
 * mutually exclusive with the semaphores that guard them. RetroCore states the
 * rule as "half a lock is no lock"
 * ($RETROCORE/Emulated.HW/ND/CPU/NDBUS/MpmWindow.cs:26-33).
 *
 * Under __EMSCRIPTEN__ the lock compiles to nothing. There is one WebAssembly
 * memory and no threads, so there is nothing to serialize; the calls remain in
 * the source so the native and browser builds are the same code.
 */

#ifndef NDBUS_LOCK_H
#define NDBUS_LOCK_H

#include "ndbus_pool.h"
#include "ndbus_types.h"

/**
 * @brief Take the bus-wide lock-cycle mutex.
 * @return Nothing.
 * @note Recursion is NOT supported: a function that takes it must not call
 *       another that does.
 */
void ndbus_lock(void);

/**
 * @brief Release the bus-wide lock-cycle mutex.
 * @return Nothing.
 * @note Recursion is NOT supported: a function that takes it must not call
 *       another that does.
 */
void ndbus_unlock(void);

/**
 * @brief THE ND-500 TSET INSTRUCTION, as the bus sees it.
 *
 * src/cpu/instructions/CONTROL/Tset.c, opcode 0xFD40: a 32-bit (W) operand,
 * writes 0xFFFFFFFF, and sets Z = (old == 0). That is the entire primitive and
 * the only place mutual exclusion is genuinely required.
 *
 * @param pool         The shared pool holding the semaphore cell.
 * @param offset       Byte offset of the 32-bit cell within the pool.
 * @param out_previous Receives the 32-bit value that was there, whether or not
 *                     the cell was taken; pass NULL if it is not wanted.
 * @return true when the cell was zero and is now held - the caller sets Z from
 *         this. An offset outside the pool returns false with
 *         *out_previous = 0 and writes nothing.
 */
bool ndbus_tset32(NdbusPool *pool, uint32_t offset, uint32_t *out_previous);

/**
 * @brief The 16-bit form, for the SINTRAN-side semaphores that live in the
 *        window (the ND-5000 X5SEM, the NUCLEUS TSET port lock).
 *
 * @param pool        The shared pool holding the semaphore cell.
 * @param offset      Byte offset of the 16-bit cell within the pool.
 * @param taken_value What is stored on success, because the two users differ:
 *                    the NUCLEUS port lock stores the real value 070000B,
 *                    X5SEM stores a host-internal marker.
 * @return true when the cell was zero and is now held; false when it was
 *         already non-zero, or when the offset is outside the pool, in which
 *         case nothing is written.
 */
bool ndbus_tset16(NdbusPool *pool, uint32_t offset, uint16_t taken_value);

/**
 * @brief Release a 16-bit semaphore cell: store zero under the same mutex.
 * @param pool   The shared pool holding the semaphore cell.
 * @param offset Byte offset of the 16-bit cell within the pool.
 * @return Nothing; an offset outside the pool writes nothing.
 */
void ndbus_semaphore_release16(NdbusPool *pool, uint32_t offset);

/**
 * @brief Release a 32-bit semaphore cell: store zero under the same mutex.
 * @param pool   The shared pool holding the semaphore cell.
 * @param offset Byte offset of the 32-bit cell within the pool.
 * @return Nothing; an offset outside the pool writes nothing.
 */
void ndbus_semaphore_release32(NdbusPool *pool, uint32_t offset);

/**
 * @brief THE SPIN DAMPER - an emulator problem with no hardware analogue.
 *
 * A guest waiting on a semaphore runs TSET / branch / TSET / ... at emulated
 * speed and burns a whole host core. On a real machine that CPU is simply
 * spinning on its own silicon and costs the other CPUs nothing; here it can
 * starve the host thread that holds the lock it is waiting for.
 *
 * One damper per attached CPU. Feed it the offset of EVERY failed TSET; when the
 * same CPU fails on the SAME address repeatedly, past a small threshold it calls
 * the host's yield(). The same-address test matters: a CPU making progress
 * through different cells is not spinning and must not be slowed down.
 */
typedef struct NdbusSpinDamper
{
    uint32_t last_offset;     /**< the address of the previous failure */
    uint32_t repeat_count;    /**< consecutive failures at that address */
    bool     have_last;       /**< false until the first failure is recorded */
} NdbusSpinDamper;

/** @brief Past this many consecutive failures at one address, yield on every failure. */
#define NDBUS_SPIN_YIELD_THRESHOLD 64u

/**
 * @brief Clear a spin damper back to "no failure recorded yet".
 * @param damper The per-CPU damper to reset.
 * @return Nothing.
 */
void ndbus_spin_damper_reset(NdbusSpinDamper *damper);

/**
 * @brief Record one FAILED TSET at `offset` and yield if the threshold is reached.
 * @param damper The per-CPU damper.
 * @param offset The pool offset the failed TSET targeted.
 * @param host   Host operations providing yield(). May be NULL, and host->yield
 *               may be NULL; the damper then counts but does not yield.
 * @return true when it yielded, false otherwise.
 */
bool ndbus_spin_damper_failed(NdbusSpinDamper *damper, uint32_t offset, const NdbusHostOps *host);

/**
 * @brief Record a SUCCESSFUL TSET: the CPU made progress, so the streak is broken.
 * @param damper The per-CPU damper.
 * @return Nothing.
 */
void ndbus_spin_damper_succeeded(NdbusSpinDamper *damper);

#endif /* NDBUS_LOCK_H */
