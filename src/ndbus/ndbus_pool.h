/*
 * ndbus_pool.h - the shared MPM-5 memory pool and its accessors
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * ONE pool, shared by the ND-100 and every attached ND-5000. The ND-5000 has no
 * private RAM: it executes out of this pool. The ND-100's own local memory is a
 * separate array that the ND-5000 cannot reach at all.
 *
 * WHY THE ACCESSORS EXIST (architecture document section 7.5.2)
 *
 * There is no multiport memory here - there is a uint8_t* and a set of host
 * threads. The host's cache-coherence fabric already does everything the MPM's
 * per-cycle arbitration did, and naturally-aligned byte accesses are single-copy
 * atomic on x86-64 and AArch64. What the host does NOT give us is a defined C
 * program: a plain uint8_t* shared between threads is a data race, and C11 lets
 * the compiler tear, fuse, duplicate or invent such accesses.
 *
 * So every guest-visible scalar access goes through ndbus_pool_r8 / _w8, which
 * are __ATOMIC_RELAXED builtins. On x86-64 and AArch64 they emit exactly the
 * same movzbl / movb (ldrb / strb) as a plain dereference: defined behaviour,
 * identical machine code, NO LOCK. QEMU solves the same problem the same way for
 * multi-threaded TCG guest RAM.
 *
 * The backing store stays a plain uint8_t* - NOT _Atomic unsigned char[], which
 * would make memcpy and every bulk transfer ill-formed and buy nothing over the
 * builtins.
 *
 * Ordinary loads and stores take no lock on either side, because the hardware
 * gave them none. Exactly three things get real synchronization: TSET
 * (ndbus_lock.h), the doorbell (ndbus_doorbell.h) and the TLB shootdown (in
 * nd500x, not here).
 */

#ifndef NDBUS_POOL_H
#define NDBUS_POOL_H

#include "ndbus_types.h"

typedef struct NdbusPool
{
    uint8_t *bytes;  /* the backing store; NULL until ndbus_pool_create */
    uint32_t size;   /* pool size in BYTES */
} NdbusPool;

/* Allocate a zeroed pool of `size_bytes`. Returns false and leaves the struct
 * zeroed on a zero size or an allocation failure. */
bool ndbus_pool_create(NdbusPool *pool, uint32_t size_bytes);

/* Free the backing store and zero the struct. Safe on an already-destroyed or
 * never-created pool. */
void ndbus_pool_destroy(NdbusPool *pool);

/* True when [offset, offset+length) lies wholly inside the pool. Overflow-safe:
 * offset + length is computed in 64 bits, so a range that wraps 2^32 is refused
 * rather than silently accepted. */
bool ndbus_pool_contains(const NdbusPool *pool, uint32_t offset, uint32_t length);

/* ---- the two primitive accessors -------------------------------------------
 *
 * Both take a pointer INTO the pool, already bounds-checked by the caller. They
 * are the only place a guest-visible byte of the pool is touched.
 */

static inline uint8_t ndbus_pool_r8(const uint8_t *p)
{
    return __atomic_load_n(p, __ATOMIC_RELAXED);
}

static inline void ndbus_pool_w8(uint8_t *p, uint8_t v)
{
    __atomic_store_n(p, v, __ATOMIC_RELAXED);
}

/* ---- bounds-guarded byte access --------------------------------------------
 *
 * An out-of-pool read returns 0 and an out-of-pool write is refused and reports
 * false, rather than wrapping through an address mask. A station has no DMA
 * fallback to fall back on, so a silent wrap turns a configuration error into
 * corrupted memory somewhere else entirely (RetroCore's octobus O1-2 review
 * item, $RETROCORE/Emulated.HW/ND/CPU/NDBUS/MpmWindow.cs).
 */
uint8_t ndbus_pool_read8(const NdbusPool *pool, uint32_t offset);
bool    ndbus_pool_write8(NdbusPool *pool, uint32_t offset, uint8_t value);

/* ---- multi-byte guest access -----------------------------------------------
 *
 * BIG-ENDIAN, because that is what both machines see: the ND-100 reads the pool
 * as 16-bit words high byte first, and the ND-500 is big-endian throughout. The
 * 32-bit form is two big-endian 16-bit words, HIGH WORD FIRST - the ND double
 * order.
 *
 * THESE MAY TEAR, AND THAT IS CORRECT. The ND-500 is byte-addressed, so a 32-bit
 * reference can land on an odd byte, which is not single-copy atomic on any
 * host - and it is not on the real machine either: ND-10.004.01 T21-T22, the MPM
 * is 32 bits wide with interleave, so an unaligned reference is more than one
 * memory cycle and another port can land between them. Do NOT add a lock here; a
 * lock would be LESS faithful, not more.
 *
 * A read whose range leaves the pool returns 0; a write whose range leaves the
 * pool is refused whole - it does not write the bytes that would have fitted.
 */
uint16_t ndbus_pool_read16(const NdbusPool *pool, uint32_t offset);
bool     ndbus_pool_write16(NdbusPool *pool, uint32_t offset, uint16_t value);
uint32_t ndbus_pool_read32(const NdbusPool *pool, uint32_t offset);
bool     ndbus_pool_write32(NdbusPool *pool, uint32_t offset, uint32_t value);

/* ---- bulk transfer ---------------------------------------------------------
 *
 * Image load and DMA. These are NOT guest-visible scalar accesses - no other CPU
 * is looking at the range while a program image is being loaded into it - so
 * they use memcpy rather than a byte-at-a-time relaxed loop, which is why the
 * backing store is a plain uint8_t* in the first place. Both refuse a range that
 * leaves the pool and copy nothing.
 */
bool ndbus_pool_read_bytes(const NdbusPool *pool, uint32_t offset, void *dst, uint32_t length);
bool ndbus_pool_write_bytes(NdbusPool *pool, uint32_t offset, const void *src, uint32_t length);

#endif /* NDBUS_POOL_H */
