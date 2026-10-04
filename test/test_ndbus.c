/*
 * test_ndbus.c - MFbus unit tests: the shared pool, the lock cycle, the doorbell
 *                and the octobus fabric.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Test-plan layers 1, 3 and 4. NO EMULATOR IS LINKED: this executable links
 * nd500_ndbus and nothing else, which is the whole point of the vtable rule in
 * src/ndbus/ndbus_types.h. Two mock CPUs stand in for the ND-100 and an ND-5000.
 *
 * The suite runs deterministically with zero threads; the concurrency test at
 * the end is the only part that starts any, and it is skipped under Emscripten
 * where there are none.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ndbus_accp.h"
#include "ndbus_context.h"
#include "ndbus_cpunum.h"
#include "ndbus_doorbell.h"
#include "ndbus_nd5000.h"
#include "ndbus_servicer.h"
#include "ndbus_lock.h"
#include "ndbus_mailbox.h"
#include "ndbus_msgqueue.h"
#include "ndbus_octobus.h"
#include "ndbus_pool.h"
#include "ndbus_testproto.h"
#include "ndbus_runner.h"
#include "ndbus_window.h"

#ifndef __EMSCRIPTEN__
#include <pthread.h>
#endif

static int s_failed = 0;
static int s_checks = 0;

#define CHECK(cond, msg)                                                                 \
    do                                                                                   \
    {                                                                                    \
        s_checks++;                                                                      \
        if (!(cond))                                                                     \
        {                                                                                \
            printf("  FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);                   \
            s_failed++;                                                                  \
        }                                                                                \
    } while (0)

#define POOL_BYTES 4096u

/* ------------------------------------------------------------------------- */
/* Layer 3: the shared pool                                                   */
/* ------------------------------------------------------------------------- */

static void test_pool(void)
{
    printf("Layer 3: shared pool\n");

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, POOL_BYTES), "pool creates");
    CHECK(pool.size == POOL_BYTES, "pool reports its size");
    CHECK(ndbus_pool_read8(&pool, 0) == 0, "a new pool is zeroed");

    /* Big-endian both ways: what one side writes as a word, the other reads as
     * the same two bytes in the same order. This is the "13B zero readback"
     * lesson - one backing array, two ports. */
    CHECK(ndbus_pool_write16(&pool, 0x100, 0x1234), "16-bit write");
    CHECK(ndbus_pool_read8(&pool, 0x100) == 0x12, "high byte first");
    CHECK(ndbus_pool_read8(&pool, 0x101) == 0x34, "low byte second");
    CHECK(ndbus_pool_read16(&pool, 0x100) == 0x1234, "16-bit readback");

    /* The reverse direction: bytes written one at a time read back as a word. */
    CHECK(ndbus_pool_write8(&pool, 0x200, 0xAB), "byte write, high");
    CHECK(ndbus_pool_write8(&pool, 0x201, 0xCD), "byte write, low");
    CHECK(ndbus_pool_read16(&pool, 0x200) == 0xABCD, "bytes read back as a word");

    /* 32-bit is two big-endian words, HIGH WORD FIRST - the ND double order. */
    CHECK(ndbus_pool_write32(&pool, 0x300, 0xDEADBEEFu), "32-bit write");
    CHECK(ndbus_pool_read8(&pool, 0x300) == 0xDE, "double byte 0");
    CHECK(ndbus_pool_read8(&pool, 0x301) == 0xAD, "double byte 1");
    CHECK(ndbus_pool_read8(&pool, 0x302) == 0xBE, "double byte 2");
    CHECK(ndbus_pool_read8(&pool, 0x303) == 0xEF, "double byte 3");
    CHECK(ndbus_pool_read32(&pool, 0x300) == 0xDEADBEEFu, "32-bit readback");
    CHECK(ndbus_pool_read16(&pool, 0x300) == 0xDEAD, "high word first");
    CHECK(ndbus_pool_read16(&pool, 0x302) == 0xBEEF, "low word second");

    /* UNALIGNED IS LEGAL. The ND-500 is byte-addressed, so a 32-bit reference
     * can land on an odd byte - and the real MPM lets it, at the cost of more
     * than one memory cycle. */
    CHECK(ndbus_pool_write32(&pool, 0x401, 0x01020304u), "unaligned 32-bit write");
    CHECK(ndbus_pool_read32(&pool, 0x401) == 0x01020304u, "unaligned 32-bit readback");
    CHECK(ndbus_pool_read8(&pool, 0x400) == 0, "the byte before is untouched");
    CHECK(ndbus_pool_read8(&pool, 0x405) == 0, "the byte after is untouched");

    /* Out of pool: read 0, write refused, NOTHING wrapped through a mask. */
    CHECK(ndbus_pool_read8(&pool, POOL_BYTES) == 0, "read past the end is 0");
    CHECK(!ndbus_pool_write8(&pool, POOL_BYTES, 0xFF), "write past the end is refused");
    CHECK(ndbus_pool_read8(&pool, 0) == 0, "the refused write did not wrap to offset 0");

    /* A range that STRADDLES the end is refused whole - it must not write the
     * bytes that would have fitted. */
    CHECK(!ndbus_pool_write16(&pool, POOL_BYTES - 1, 0xBEEF), "straddling 16-bit write refused");
    CHECK(ndbus_pool_read8(&pool, POOL_BYTES - 1) == 0, "the byte that would have fitted is clean");
    CHECK(!ndbus_pool_write32(&pool, POOL_BYTES - 3, 0), "straddling 32-bit write refused");
    CHECK(ndbus_pool_read16(&pool, POOL_BYTES - 1) == 0, "straddling 16-bit read is 0");

    /* Overflow-safe bounds: a range that wraps 2^32 must be refused, not
     * accepted because the sum came out small. */
    CHECK(!ndbus_pool_contains(&pool, 0xFFFFFFF0u, 0x20), "a wrapping range is refused");
    CHECK(ndbus_pool_contains(&pool, 0, POOL_BYTES), "the whole pool is contained");
    CHECK(!ndbus_pool_contains(&pool, 0, POOL_BYTES + 1), "one byte too many is not");

    /* Bulk transfer: the image-load and DMA path. */
    uint8_t src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t dst[8];
    memset(dst, 0, sizeof(dst));
    CHECK(ndbus_pool_write_bytes(&pool, 0x500, src, sizeof(src)), "bulk write");
    CHECK(ndbus_pool_read_bytes(&pool, 0x500, dst, sizeof(dst)), "bulk read");
    CHECK(memcmp(src, dst, sizeof(src)) == 0, "bulk round trip");
    CHECK(!ndbus_pool_write_bytes(&pool, POOL_BYTES - 4, src, sizeof(src)),
          "bulk write past the end is refused");
    CHECK(ndbus_pool_read8(&pool, POOL_BYTES - 4) == 0, "the refused bulk write wrote nothing");

    ndbus_pool_destroy(&pool);
    CHECK(pool.bytes == NULL, "destroy clears the pointer");
    ndbus_pool_destroy(&pool); /* must be safe twice */

    NdbusPool empty;
    CHECK(!ndbus_pool_create(&empty, 0), "a zero-size pool is refused");
    CHECK(!ndbus_pool_contains(&empty, 0, 1), "an uncreated pool contains nothing");
}

/* ------------------------------------------------------------------------- */
/* Layer 3: the lock cycle                                                    */
/* ------------------------------------------------------------------------- */

static int s_yield_count = 0;

static void mock_yield(void *ctx)
{
    (void)ctx;
    s_yield_count++;
}

static void test_lock_cycle(void)
{
    printf("Layer 3: TSET and the lock cycle\n");

    NdbusPool pool;
    (void)ndbus_pool_create(&pool, POOL_BYTES);

    /* TSET: 32-bit, writes all ones, taken only when the cell was zero. */
    uint32_t previous = 0xFFu;
    CHECK(ndbus_tset32(&pool, 0x10, &previous), "TSET on a free cell takes it");
    CHECK(previous == 0, "the previous value was zero");
    CHECK(ndbus_pool_read32(&pool, 0x10) == 0xFFFFFFFFu, "TSET writes all ones, not 1");

    CHECK(!ndbus_tset32(&pool, 0x10, &previous), "TSET on a held cell fails");
    CHECK(previous == 0xFFFFFFFFu, "the previous value is reported on failure too");
    CHECK(ndbus_pool_read32(&pool, 0x10) == 0xFFFFFFFFu, "a failed TSET does not change the cell");

    ndbus_semaphore_release32(&pool, 0x10);
    CHECK(ndbus_pool_read32(&pool, 0x10) == 0, "release stores zero");
    CHECK(ndbus_tset32(&pool, 0x10, NULL), "the cell can be taken again");
    CHECK(ndbus_tset32(&pool, 0x14, NULL), "a different cell is independent");

    /* A non-zero cell is held whatever the value - TSET tests against zero, not
     * against its own marker. */
    (void)ndbus_pool_write32(&pool, 0x20, 1);
    CHECK(!ndbus_tset32(&pool, 0x20, NULL), "any non-zero value means held");

    /* Unaligned, because the guest's operand need not be aligned. */
    CHECK(ndbus_tset32(&pool, 0x31, NULL), "TSET works on an unaligned cell");
    CHECK(ndbus_pool_read32(&pool, 0x31) == 0xFFFFFFFFu, "and writes all ones there");

    /* Out of pool: refused, nothing written. */
    CHECK(!ndbus_tset32(&pool, POOL_BYTES - 2, &previous), "TSET straddling the end is refused");
    CHECK(previous == 0, "and reports no previous value");

    /* The 16-bit form, with the caller's taken value: the NUCLEUS port lock
     * stores the real 070000B, X5SEM stores a host-internal marker. */
    CHECK(ndbus_tset16(&pool, 0x40, 0x7000), "16-bit TSET takes a free cell");
    CHECK(ndbus_pool_read16(&pool, 0x40) == 0x7000, "it stores the caller's taken value");
    CHECK(!ndbus_tset16(&pool, 0x40, 0x7000), "and fails on a held cell");
    ndbus_semaphore_release16(&pool, 0x40);
    CHECK(ndbus_tset16(&pool, 0x40, 0xFFFF), "released, it takes again with another marker");

    /* The spin damper. A guest busy-waiting on a semaphore has no hardware
     * analogue and would burn a whole host core; the damper yields only when the
     * SAME CPU fails repeatedly at the SAME address. */
    NdbusHostOps host;
    memset(&host, 0, sizeof(host));
    host.yield = mock_yield;

    NdbusSpinDamper damper;
    ndbus_spin_damper_reset(&damper);
    s_yield_count = 0;

    for (unsigned i = 0; i < NDBUS_SPIN_YIELD_THRESHOLD - 1u; i++)
    {
        (void)ndbus_spin_damper_failed(&damper, 0x40, &host);
    }
    CHECK(s_yield_count == 0, "below the threshold the damper does not yield");
    CHECK(ndbus_spin_damper_failed(&damper, 0x40, &host), "at the threshold it yields");
    CHECK(s_yield_count == 1, "exactly once");
    CHECK(ndbus_spin_damper_failed(&damper, 0x40, &host), "and on every failure after that");
    CHECK(s_yield_count == 2, "twice");

    /* A different address means progress, not spinning: the streak restarts and
     * a CPU walking a list of semaphores is never slowed down. */
    CHECK(!ndbus_spin_damper_failed(&damper, 0x44, &host), "a new address restarts the streak");
    CHECK(s_yield_count == 2, "and does not yield");

    /* Success also breaks the streak. */
    ndbus_spin_damper_reset(&damper);
    for (unsigned i = 0; i < NDBUS_SPIN_YIELD_THRESHOLD; i++)
    {
        (void)ndbus_spin_damper_failed(&damper, 0x50, &host);
    }
    int after_threshold = s_yield_count;
    ndbus_spin_damper_succeeded(&damper);
    CHECK(!ndbus_spin_damper_failed(&damper, 0x50, &host), "after a success the streak is over");
    CHECK(s_yield_count == after_threshold, "so it does not yield again immediately");

    /* A NULL host, and a host with no yield, must both be safe. */
    ndbus_spin_damper_reset(&damper);
    for (unsigned i = 0; i < NDBUS_SPIN_YIELD_THRESHOLD + 2u; i++)
    {
        (void)ndbus_spin_damper_failed(&damper, 0x60, NULL);
    }
    memset(&host, 0, sizeof(host));
    ndbus_spin_damper_reset(&damper);
    for (unsigned i = 0; i < NDBUS_SPIN_YIELD_THRESHOLD + 2u; i++)
    {
        (void)ndbus_spin_damper_failed(&damper, 0x60, &host);
    }
    CHECK(true, "a NULL host and a host without yield are both safe");

    ndbus_pool_destroy(&pool);
}

/* ------------------------------------------------------------------------- */
/* Layer 3: the doorbell                                                      */
/* ------------------------------------------------------------------------- */

static void test_doorbell(void)
{
    printf("Layer 3: doorbell\n");

    NdbusPool pool;
    (void)ndbus_pool_create(&pool, POOL_BYTES);

    CHECK(ndbus_doorbell_read(&pool, 0x80) == 0, "a fresh doorbell is clear");
    CHECK(ndbus_doorbell_ring(&pool, 0x80, 1), "ringing succeeds");
    CHECK(ndbus_doorbell_read(&pool, 0x80) == 1, "and is visible");
    CHECK(ndbus_doorbell_read(&pool, 0x80) == 1, "reading does not clear it");
    CHECK(ndbus_doorbell_take(&pool, 0x80) == 1, "taking returns the value");
    CHECK(ndbus_doorbell_read(&pool, 0x80) == 0, "and clears it");
    CHECK(ndbus_doorbell_take(&pool, 0x80) == 0, "taking a clear doorbell returns 0");

    /* The doorbell is an ordinary pool byte - the ordering is the only thing
     * that differs, so a relaxed reader sees the same cell. */
    CHECK(ndbus_doorbell_ring(&pool, 0x90, 0x5A), "ring with a value");
    CHECK(ndbus_pool_read8(&pool, 0x90) == 0x5A, "a relaxed read sees the same byte");

    CHECK(!ndbus_doorbell_ring(&pool, POOL_BYTES, 1), "ringing outside the pool is refused");
    CHECK(ndbus_doorbell_read(&pool, POOL_BYTES) == 0, "and reads 0 there");

    ndbus_pool_destroy(&pool);
}

/* ------------------------------------------------------------------------- */
/* Layer 4: the octobus                                                       */
/* ------------------------------------------------------------------------- */

typedef struct
{
    int      frames_seen;
    uint16_t last_frame;
    uint8_t  last_source;
    int      reply_count;      /* how many frames this mock answers with */
    uint16_t reply_value;
} MockStationState;

static int mock_station_handle(NdbusStation *station, uint16_t frame, uint8_t source_station,
                               uint16_t *replies)
{
    MockStationState *state = (MockStationState *)station->ctx;
    state->frames_seen++;
    state->last_frame  = frame;
    state->last_source = source_station;

    for (int i = 0; i < state->reply_count && i < NDBUS_MAX_REPLY_FRAMES; i++)
    {
        replies[i] = (uint16_t)(state->reply_value + (uint16_t)i);
    }
    return state->reply_count;
}

static void test_octobus(void)
{
    printf("Layer 4: octobus fabric\n");

    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);

    CHECK(ndbus_fabric_station_count(&fabric) == 0, "a fresh fabric has no stations");
    CHECK(ndbus_fabric_master(&fabric) == 0, "and no MASTER - XRFO not pulsing");

    MockStationState nd120_state;
    MockStationState nd5000_state;
    memset(&nd120_state, 0, sizeof(nd120_state));
    memset(&nd5000_state, 0, sizeof(nd5000_state));

    NdbusStation nd120 = {NDBUS_STATION_ND120_CPU, "ND-120 CPU", mock_station_handle,
                          &nd120_state, NULL};
    NdbusStation nd5000 = {NDBUS_STATION_ND5000_FIRST, "ND-5000 CPU", mock_station_handle,
                           &nd5000_state, NULL};

    CHECK(ndbus_fabric_register(&fabric, &nd5000), "station 70B registers");
    CHECK(ndbus_fabric_master(&fabric) == NDBUS_STATION_ND5000_FIRST,
          "the only station is the MASTER");
    CHECK(ndbus_fabric_register(&fabric, &nd120), "station 1B registers");
    CHECK(ndbus_fabric_master(&fabric) == NDBUS_STATION_ND120_CPU,
          "the LOWEST station number becomes MASTER");
    CHECK(ndbus_fabric_station_count(&fabric) == 2, "two stations on the bus");

    /* Station 0 and 77B are not in the T329 table. */
    NdbusStation illegal_low  = {0, "illegal 0", NULL, NULL, NULL};
    NdbusStation illegal_high = {63, "illegal 77B", NULL, NULL, NULL};
    CHECK(!ndbus_fabric_register(&fabric, &illegal_low), "station 0 is refused");
    CHECK(!ndbus_fabric_register(&fabric, &illegal_high), "station 77B is refused");
    CHECK(ndbus_fabric_station_count(&fabric) == 2, "a refused registration changes nothing");

    /* All seven ND-5000 slots, 70B..76B, and no eighth. */
    NdbusStation     extra[NDBUS_ND5000_MAX_CPUS];
    MockStationState extra_state[NDBUS_ND5000_MAX_CPUS];
    memset(extra_state, 0, sizeof(extra_state));
    for (unsigned i = 1; i < NDBUS_ND5000_MAX_CPUS; i++)
    {
        extra[i].number = (uint8_t)(NDBUS_STATION_ND5000_FIRST + i);
        extra[i].type   = "ND-5000 CPU";
        extra[i].handle = mock_station_handle;
        extra[i].ctx    = &extra_state[i];
        CHECK(ndbus_fabric_register(&fabric, &extra[i]), "an ND-5000 slot registers");
    }
    CHECK(ndbus_fabric_station_count(&fabric) == 1 + (int)NDBUS_ND5000_MAX_CPUS,
          "seven ND-5000 slots plus the ND-120");
    CHECK(NDBUS_STATION_ND5000_LAST == 62, "76B is 62 decimal");
    NdbusStation eighth = {63, "an eighth ND-5000", NULL, NULL, NULL};
    CHECK(!ndbus_fabric_register(&fabric, &eighth), "there is no eighth slot");

    /* Duplicate registration is refused rather than silently replacing, so two
     * owners fighting over one slot cannot hide. */
    NdbusStation duplicate = {NDBUS_STATION_ND5000_FIRST, "another 70B", NULL, NULL, NULL};
    CHECK(!ndbus_fabric_register(&fabric, &duplicate), "a duplicate station number is refused");
    CHECK(ndbus_fabric_get_station(&fabric, NDBUS_STATION_ND5000_FIRST) == &nd5000,
          "and the original station is still there");

    /* THE DESTINATION-TO-SOURCE REWRITE. The frame goes out carrying the
     * destination in bits 13-8 and arrives carrying the sender. */
    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
    uint16_t dest_70b = (uint16_t)(NDBUS_STATION_ND5000_FIRST << NDBUS_FRAME_STATION_SHIFT);
    uint16_t frame = (uint16_t)(NDBUS_FRAME_C_CONTROL | dest_70b | 0x05u);
    nd5000_state.reply_count = 0;
    int sent = ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, frame, replies);
    CHECK(sent == 0, "delivered, no reply");
    CHECK(nd5000_state.frames_seen == 1, "the addressed station saw exactly one frame");
    CHECK(ndbus_frame_station(nd5000_state.last_frame) == NDBUS_STATION_ND120_CPU,
          "bits 13-8 were rewritten to the SOURCE on delivery");
    CHECK(nd5000_state.last_source == NDBUS_STATION_ND120_CPU,
          "and the source is passed alongside");
    CHECK((nd5000_state.last_frame & NDBUS_FRAME_C_CONTROL) != 0, "the C bit survived the rewrite");
    CHECK((nd5000_state.last_frame & NDBUS_FRAME_CODE_MASK) == 0x05u, "the code field survived");
    CHECK(nd120_state.frames_seen == 0, "the sender did not receive its own frame");

    /* Replies come back. */
    nd5000_state.reply_count = 2;
    nd5000_state.reply_value = 0x00A0;
    sent                     = ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, frame, replies);
    CHECK(sent == 2, "two reply frames");
    CHECK(replies[0] == 0x00A0 && replies[1] == 0x00A1, "in order");

    /* TIMEOUT is -1, and it is NOT the same as "delivered, no reply". An absent
     * station must never look like a quiet one. */
    uint16_t to_empty = (uint16_t)(40u << NDBUS_FRAME_STATION_SHIFT);
    CHECK(ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, to_empty, replies) == -1,
          "an unregistered station times out");
    uint16_t to_zero = 0;
    CHECK(ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, to_zero, replies) == -1,
          "destination 0 times out");

    /* A registered station with no handler accepts and is silent - which is 0,
     * not a timeout. */
    CHECK(ndbus_fabric_unregister(&fabric, NDBUS_STATION_ND5000_FIRST), "70B unregisters");
    NdbusStation silent = {NDBUS_STATION_ND5000_FIRST, "silent", NULL, NULL, NULL};
    CHECK(ndbus_fabric_register(&fabric, &silent), "a handler-less station registers");
    CHECK(ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, frame, replies) == 0,
          "it accepts and says nothing - 0, not a timeout");

    /* Broadcast reaches every station except the sender. */
    CHECK(ndbus_fabric_unregister(&fabric, NDBUS_STATION_ND5000_FIRST), "silent station removed");
    CHECK(ndbus_fabric_register(&fabric, &nd5000), "70B back on the bus");
    nd5000_state.frames_seen = 0;
    nd5000_state.reply_count = 0;
    nd120_state.frames_seen  = 0;
    nd120_state.reply_count  = 0;
    for (unsigned i = 1; i < NDBUS_ND5000_MAX_CPUS; i++)
    {
        extra_state[i].frames_seen = 0;
        extra_state[i].reply_count = 0;
    }
    uint16_t broadcast = (uint16_t)(NDBUS_FRAME_B_BROADCAST | NDBUS_FRAME_C_CONTROL);
    sent               = ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, broadcast, replies);
    CHECK(sent == 0, "no station replied to the broadcast");
    CHECK(nd120_state.frames_seen == 0, "the sender is excluded from its own broadcast");
    CHECK(nd5000_state.frames_seen == 1, "70B received the broadcast");
    CHECK(extra_state[NDBUS_ND5000_MAX_CPUS - 1].frames_seen == 1, "so did 76B");

    /* Unregistering. */
    CHECK(!ndbus_fabric_unregister(&fabric, 40), "unregistering an empty slot fails");
    CHECK(ndbus_fabric_has_station(&fabric, NDBUS_STATION_ND120_CPU), "1B is on the bus");
    CHECK(ndbus_fabric_unregister(&fabric, NDBUS_STATION_ND120_CPU), "1B unregisters");
    CHECK(!ndbus_fabric_has_station(&fabric, NDBUS_STATION_ND120_CPU), "and is gone");
    CHECK(ndbus_fabric_master(&fabric) == NDBUS_STATION_ND5000_FIRST,
          "the MASTER moves to the next lowest station");

    /* Arbitration priority: a station that gives up is incremented, so it
     * eventually wins and nothing starves. */
    NdbusArbiter arbiter;
    ndbus_arbiter_reset(&arbiter);
    CHECK(arbiter.priority == 0, "priority starts at 0");
    CHECK(ndbus_arbiter_lost(&arbiter) == 1, "losing increments it");
    CHECK(ndbus_arbiter_lost(&arbiter) == 2, "and again");
    ndbus_arbiter_won(&arbiter);
    CHECK(arbiter.priority == 0, "winning resets it");
    for (int i = 0; i < 100; i++)
    {
        (void)ndbus_arbiter_lost(&arbiter);
    }
    CHECK(arbiter.priority == NDBUS_PRIORITY_MAX,
          "the 4-bit priority saturates rather than wrapping to 0");

    CHECK(ndbus_arbitrate(5, 3, 9, 1) == 5, "the higher priority wins");
    CHECK(ndbus_arbitrate(5, 1, 9, 3) == 9, "whichever station holds it");
    CHECK(ndbus_arbitrate(5, 2, 9, 2) == 5, "on a tie the lower station number wins");
    CHECK(ndbus_arbitrate(9, 2, 5, 2) == 5, "regardless of argument order");
}

/* ------------------------------------------------------------------------- */
/* Layer 3: TSET under real concurrency                                       */
/* ------------------------------------------------------------------------- */

#ifndef __EMSCRIPTEN__

#define TSET_THREADS    4
#define TSET_ITERATIONS 20000

typedef struct
{
    NdbusPool *pool;
    long       acquisitions;
} TsetWorker;

/* Each thread takes the semaphore, increments a counter that lives IN THE POOL,
 * and releases. If the lock cycle were not atomic the counter would come out
 * short - that is the whole test. */
static void *tset_worker(void *arg)
{
    TsetWorker *worker = (TsetWorker *)arg;
    for (int i = 0; i < TSET_ITERATIONS; i++)
    {
        while (!ndbus_tset32(worker->pool, 0x10, NULL))
        {
            /* spin; the damper is the guest's concern, not the test's */
        }
        uint32_t counter = ndbus_pool_read32(worker->pool, 0x20);
        (void)ndbus_pool_write32(worker->pool, 0x20, counter + 1);
        worker->acquisitions++;
        ndbus_semaphore_release32(worker->pool, 0x10);
    }
    return NULL;
}

static void test_tset_concurrency(void)
{
    printf("Layer 3: TSET under %d threads\n", TSET_THREADS);

    NdbusPool pool;
    (void)ndbus_pool_create(&pool, POOL_BYTES);

    pthread_t  threads[TSET_THREADS];
    TsetWorker workers[TSET_THREADS];

    for (int i = 0; i < TSET_THREADS; i++)
    {
        workers[i].pool         = &pool;
        workers[i].acquisitions = 0;
        if (pthread_create(&threads[i], NULL, tset_worker, &workers[i]) != 0)
        {
            printf("  SKIP: could not create thread %d\n", i);
            for (int j = 0; j < i; j++)
            {
                (void)pthread_join(threads[j], NULL);
            }
            ndbus_pool_destroy(&pool);
            return;
        }
    }

    long total = 0;
    for (int i = 0; i < TSET_THREADS; i++)
    {
        (void)pthread_join(threads[i], NULL);
        total += workers[i].acquisitions;
    }

    CHECK(total == (long)TSET_THREADS * TSET_ITERATIONS, "every acquisition completed");
    CHECK(ndbus_pool_read32(&pool, 0x20) == (uint32_t)total,
          "the in-pool counter matches - no lost update, so the lock cycle is atomic");
    CHECK(ndbus_pool_read32(&pool, 0x10) == 0, "the semaphore is free at the end");

    ndbus_pool_destroy(&pool);
}

#endif /* __EMSCRIPTEN__ */

/* ------------------------------------------------------------------------- */
/* Layer 5: the ACCP command layer                                            */
/* ------------------------------------------------------------------------- */

static NdbusAccpState accp_state(bool running, bool syspar, bool pointer, bool kicks)
{
    NdbusAccpState s;
    s.microprogram_running    = running;
    s.system_parameters_given = syspar;
    s.parameter_pointer_given = pointer;
    s.kicks_enabled           = kicks;
    return s;
}

static void test_accp(void)
{
    printf("Layer 5: ACCP command guards\n");

    /* The dispatcher arm range and its four holes. */
    CHECK(!ndbus_accp_has_arm(0x0C), "0x0C is below the range");
    CHECK(ndbus_accp_has_arm(0x0D), "0x0D is the first arm");
    CHECK(ndbus_accp_has_arm(0x3E), "0x3E is the last arm");
    CHECK(!ndbus_accp_has_arm(0x3F), "0x3F is above the range");
    CHECK(!ndbus_accp_has_arm(0x19), "0x19 is a hole");
    CHECK(!ndbus_accp_has_arm(0x1A), "0x1A is a hole");
    CHECK(!ndbus_accp_has_arm(0x2E), "0x2E is a hole");
    CHECK(!ndbus_accp_has_arm(0x2F), "0x2F is a hole");

    NdbusAccpState idle    = accp_state(false, false, false, false);
    NdbusAccpState ready   = accp_state(false, true, true, false);
    NdbusAccpState running = accp_state(true, true, true, false);
    NdbusAccpState kicking = accp_state(false, true, true, true);

    /* A code with no arm naks 6 whatever the state. */
    CHECK(ndbus_accp_evaluate(0x19, &idle) == NDBUS_ACCP_NAK_UNDEFINED_COMMAND,
          "a hole naks 6");
    CHECK(ndbus_accp_evaluate(0xFF, &ready) == NDBUS_ACCP_NAK_UNDEFINED_COMMAND,
          "so does anything outside the range");

    /* The bring-up set is accepted from the idle state. */
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_ECHO, &idle) == NDBUS_ACCP_ACCEPTED,
          "ECHO is accepted cold");
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_LSYSPAR, &idle) == NDBUS_ACCP_ACCEPTED,
          "LSYSPAR is accepted cold");
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_LPARP, &idle) == NDBUS_ACCP_ACCEPTED,
          "LPARP is accepted cold");
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_CPURES, &idle) == NDBUS_ACCP_ACCEPTED,
          "CPURES is accepted cold");
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_STARTMIC, &ready) == NDBUS_ACCP_ACCEPTED,
          "STARTMIC is accepted when not running");

    /* No parameter pointer yet. */
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_VPARP, &idle) == NDBUS_ACCP_NAK_NO_PARAM_POINTER,
          "VPARP without a pointer naks 1");
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_VPARP, &ready) == NDBUS_ACCP_ACCEPTED,
          "and is accepted once the pointer is given");

    /* Illegal while the microprogram runs. */
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_STARTMIC, &running) == NDBUS_ACCP_NAK_MICRO_RUNNING,
          "STARTMIC while running naks -1");
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_VPARP, &running) == NDBUS_ACCP_NAK_MICRO_RUNNING,
          "so does VPARP");
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_ECHO, &running) == NDBUS_ACCP_ACCEPTED,
          "but ECHO still works while running");

    /* STOPMIC is the one command with the INVERSE guard. */
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_STOPMIC, &running) == NDBUS_ACCP_ACCEPTED,
          "STOPMIC while running is accepted");
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_STOPMIC, &ready) == NDBUS_ACCP_NAK_MICRO_NOT_STARTED,
          "STOPMIC while stopped naks 0");

    /* Kicks. */
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_RAIB32D, &kicking) == NDBUS_ACCP_NAK_KICKS_ENABLED,
          "reading the AIB with kicks enabled naks -2");
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_RAIB32D, &ready) == NDBUS_ACCP_ACCEPTED,
          "and is fine with kicks disabled");

    /* Reading the system parameters back before they were loaded: the
     * undocumented code 13, in the short two-byte form. */
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_RSSYSPAR, &idle) == NDBUS_ACCP_NAK_SYSPAR_NOT_GIVEN,
          "RSSYSPAR before LSYSPAR naks 13");
    CHECK(ndbus_accp_nak_is_short(NDBUS_ACCP_NAK_SYSPAR_NOT_GIVEN),
          "and 13 is the SHORT nak form");
    CHECK(!ndbus_accp_nak_is_short(NDBUS_ACCP_NAK_MICRO_RUNNING),
          "while -1 carries the two status bytes");
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_RSSYSPAR, &ready) == NDBUS_ACCP_ACCEPTED,
          "RSSYSPAR after LSYSPAR is accepted");

    /* THE GUARD ORDER. Matrix state S5 - running, no pointer given - answers -1
     * for the commands that carry the running guard and 1 only for those that do
     * not. Swapping the two blocks in ndbus_accp_evaluate() flips four of these,
     * which is why the order is a test and not a comment. */
    NdbusAccpState s5 = accp_state(true, true, false, false);
    CHECK(ndbus_accp_evaluate(0x12, &s5) == NDBUS_ACCP_NAK_MICRO_RUNNING, "S5: 0x12 answers -1");
    CHECK(ndbus_accp_evaluate(0x13, &s5) == NDBUS_ACCP_NAK_MICRO_RUNNING, "S5: 0x13 answers -1");
    CHECK(ndbus_accp_evaluate(0x15, &s5) == NDBUS_ACCP_NAK_MICRO_RUNNING, "S5: 0x15 answers -1");
    CHECK(ndbus_accp_evaluate(0x3C, &s5) == NDBUS_ACCP_NAK_MICRO_RUNNING, "S5: 0x3C answers -1");
    CHECK(ndbus_accp_evaluate(0x34, &s5) == NDBUS_ACCP_NAK_NO_PARAM_POINTER, "S5: 0x34 answers 1");
    CHECK(ndbus_accp_evaluate(0x35, &s5) == NDBUS_ACCP_NAK_NO_PARAM_POINTER, "S5: 0x35 answers 1");

    /* THE COMMAND BYTES ARE NOT THE MANUAL'S SECTION NUMBERS. The manual lists
     * Start Microprogram as section 5.3.23; the real command is 066B = 0x36, and
     * 033B is RUNSELFT. A table derived from section numbers looks right and
     * starts nothing. */
    CHECK(NDBUS_ACCP_STARTMIC == 0x36, "STARTMIC is 066B, not the section number");
    CHECK(NDBUS_ACCP_RUNTST == 0x1B, "033B is RUNTST, not STARTMIC");
    CHECK(NDBUS_ACCP_STARTMIC != 0x1B, "and the two are not the same command");

    /* Parameter lengths, and the commands that read them before the guards. */
    CHECK(ndbus_accp_min_parameter_bytes(NDBUS_ACCP_ECHO) == 1, "ECHO needs its count byte");
    CHECK(ndbus_accp_min_parameter_bytes(NDBUS_ACCP_LSYSPAR) == 6,
          "LSYSPAR takes three 16-bit words");
    CHECK(ndbus_accp_min_parameter_bytes(NDBUS_ACCP_LPARP) == 4,
          "LPARP takes a 4-byte pointer");
    CHECK(ndbus_accp_min_parameter_bytes(NDBUS_ACCP_LMIR) == 16,
          "LMIR takes the 128-bit microinstruction register");
    CHECK(ndbus_accp_min_parameter_bytes(NDBUS_ACCP_LOCSD) == 20,
          "LOCSD takes 8 words plus address and checksum");
    CHECK(ndbus_accp_min_parameter_bytes(NDBUS_ACCP_LOCSM) == 0,
          "LOCSM is sent bare - its parameters are in MFbus memory");
    CHECK(ndbus_accp_length_checked_first(NDBUS_ACCP_LOCSD),
          "LOCSD reads its parameters before the guards");
    CHECK(ndbus_accp_length_checked_first(NDBUS_ACCP_DCSD), "so does DCSD");
    CHECK(!ndbus_accp_length_checked_first(NDBUS_ACCP_STARTMIC),
          "STARTMIC does not - a bare one while running still naks -1");

    /* ALIVE is deliberately not decided by the guard table: the real card naks 7
     * from a hardware alive signal, which a state struct cannot know. */
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_ALIVE, &idle) == NDBUS_ACCP_ACCEPTED,
          "ALIVE passes the guards - the station answers it, not the table");
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_ALIVE, &running) == NDBUS_ACCP_ACCEPTED,
          "in every state");

    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_ECHO, NULL) == NDBUS_ACCP_NAK_UNDEFINED_COMMAND,
          "a NULL state is refused rather than dereferenced");
}

/* ------------------------------------------------------------------------- */
/* Layer 6: the ND-5000 station, and the X5ACT doorbell                       */
/* ------------------------------------------------------------------------- */

/* Send one ACCP command to the station through the fabric, as the ND-120 does:
 * SOMB to OMD 3, one data frame per body byte, EOMB. Returns the reply count the
 * EOMB produced. */
/*
 * Send one ACCP message the way the wire carries it.
 *
 * `payload` is the COMMAND AND ITS PARAMETERS. The two header bytes in front of
 * them - the source OMD the sender listens on, and the payload byte count - are
 * added here, because they are on the wire: captured from SINTRAN's own ND-500
 * monitor, the message to station 070B is
 *
 *     SOMB omd=3 | 03 07 0E 01 03 00 00 00 00 | EOMB omd=3
 *
 * i.e. source OMD 3, count 7, command 016B, six parameter bytes. This helper used
 * to send SOMB, the payload, EOMB and nothing else, which is the same mistake the
 * station made when reading it - so the test agreed with the bug instead of
 * catching it.
 */
static int send_accp(NdbusFabric *fabric, uint8_t from, uint8_t to, const uint8_t *payload,
                     int length, uint16_t *replies)
{
    uint16_t dest = (uint16_t)((uint16_t)to << NDBUS_FRAME_STATION_SHIFT);

    uint16_t somb = (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                               NDBUS_FRAME_S_STARTSTOP | (uint16_t)NDBUS_ACCP_OMD);
    (void)ndbus_fabric_send(fabric, from, somb, replies);

    /* Source OMD, then the byte count. */
    (void)ndbus_fabric_send(fabric, from, (uint16_t)(dest | (uint16_t)NDBUS_ACCP_OMD), replies);
    (void)ndbus_fabric_send(fabric, from, (uint16_t)(dest | (uint16_t)length), replies);

    for (int i = 0; i < length; i++)
    {
        uint16_t data = (uint16_t)(dest | (uint16_t)payload[i]);
        (void)ndbus_fabric_send(fabric, from, data, replies);
    }

    uint16_t eomb = (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                               (uint16_t)NDBUS_ACCP_OMD);
    return ndbus_fabric_send(fabric, from, eomb, replies);
}

/*
 * Unwrap a reply and hand back its payload, so a test asserts what the message
 * SAYS rather than how many frames it took.
 *
 * Checks the whole envelope, because every part of it is a way the reply can be
 * wrong and has been: SOMB present with M and S set, EOMB present with M and S
 * clear, the OMD equal in both, the byte count matching what actually arrived,
 * and - the one that was silently zero for months - EVERY frame carrying the
 * REPLYING STATION in bits 13-8.
 *
 * Returns the payload length, or -1 with a reason printed.
 */
static int accp_reply_payload(const uint16_t *replies, int n, uint8_t from_station, uint8_t *out,
                             int out_max)
{
    if (n < 4)
    {
        printf("  reply has %d frame(s): too few for an envelope\n", n);
        return -1;
    }

    for (int i = 0; i < n; i++)
    {
        if (ndbus_frame_station(replies[i]) != from_station)
        {
            printf("  reply frame %d says station %uB, expected %uB\n", i,
                   (unsigned)ndbus_frame_station(replies[i]), (unsigned)from_station);
            return -1;
        }
    }

    uint16_t somb = replies[0];
    uint16_t eomb = replies[n - 1];
    if ((somb & NDBUS_FRAME_C_CONTROL) == 0 || (somb & NDBUS_FRAME_M_MULTIBYTE) == 0 ||
        (somb & NDBUS_FRAME_S_STARTSTOP) == 0)
    {
        printf("  first frame %06o is not a SOMB\n", somb);
        return -1;
    }
    if ((eomb & NDBUS_FRAME_C_CONTROL) == 0 || (eomb & NDBUS_FRAME_M_MULTIBYTE) == 0 ||
        (eomb & NDBUS_FRAME_S_STARTSTOP) != 0)
    {
        printf("  last frame %06o is not an EOMB\n", eomb);
        return -1;
    }
    if ((somb & NDBUS_FRAME_CODE_MASK) != (eomb & NDBUS_FRAME_CODE_MASK))
    {
        printf("  SOMB OMD %u and EOMB OMD %u disagree\n",
               (unsigned)(somb & NDBUS_FRAME_CODE_MASK), (unsigned)(eomb & NDBUS_FRAME_CODE_MASK));
        return -1;
    }

    int declared = (int)(replies[2] & NDBUS_FRAME_DATA_MASK);
    int actual   = n - 4;
    if (declared != actual)
    {
        printf("  reply declares %d payload byte(s) and carries %d\n", declared, actual);
        return -1;
    }
    if (actual > out_max)
    {
        printf("  payload of %d byte(s) does not fit the caller's buffer\n", actual);
        return -1;
    }

    for (int i = 0; i < actual; i++)
    {
        out[i] = (uint8_t)(replies[3 + i] & NDBUS_FRAME_DATA_MASK);
    }
    return actual;
}


static int s_log_lines = 0;

static void counting_log(void *ctx, int level, const char *message)
{
    (void)ctx;
    (void)level;
    (void)message;
    s_log_lines++;
}

static void test_nd5000_station(void)
{
    printf("Layer 6: ND-5000 station and doorbell\n");

    NdbusPool pool;
    (void)ndbus_pool_create(&pool, POOL_BYTES);

    NdbusHostOps host;
    memset(&host, 0, sizeof(host));
    host.log = counting_log;

    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);

    NdbusNd5000 nd;
    CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, &host, NULL),
          "the station comes up at 70B");
    CHECK(ndbus_fabric_register(&fabric, &nd.station), "and registers on the fabric");

    /* A station number outside 70B..76B is configuration, not a bus condition,
     * and must fail where someone can see it - an ND-5000 answering at 10B would
     * collide with a SCSI controller. */
    NdbusNd5000 wrong;
    CHECK(!ndbus_nd5000_init(&wrong, 8, &pool, &host, NULL), "10B is refused");
    CHECK(!ndbus_nd5000_init(&wrong, NDBUS_STATION_ND120_CPU, &pool, &host, NULL),
          "1B is refused");
    CHECK(!ndbus_nd5000_init(&wrong, NDBUS_STATION_ND5000_LAST + 1, &pool, &host, NULL),
          "one past 76B is refused");
    CHECK(ndbus_nd5000_init(&wrong, NDBUS_STATION_ND5000_LAST, &pool, &host, NULL),
          "76B itself is accepted");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
    uint8_t  body[8];

    /* ECHO: accepted cold, and it needs its count byte. */
    body[0] = (uint8_t)NDBUS_ACCP_ECHO;
    body[1] = 2;
    body[2] = 0xAA;
    body[3] = 0x55;
    int n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 4,
                      replies);
    /* Messack plus the echoed pattern: T125 says returned data follows Messack
     * in the same multibyte message. */
    uint8_t payload[NDBUS_MAX_REPLY_FRAMES];
    int     plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                                      (int)sizeof(payload));
    CHECK(plen == 3 && payload[0] == 0x00 && payload[1] == 0xAA && payload[2] == 0x55,
          "ECHO is answered with a Messack carrying its two echoed bytes");
    CHECK(nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "and it is a Messack");
    CHECK(nd.messages_handled == 1, "the station counted one message");

    /* A command below its measured parameter length gets NO REPLY - not a nak.
     * The real card is silent, and a caller that turns silence into an error
     * reports a fault the hardware does not report. */
    body[0] = (uint8_t)NDBUS_ACCP_LSYSPAR; /* needs 6 parameter bytes */
    n       = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                        replies);
    CHECK(n == 0, "a short LSYSPAR gets no reply at all");
    CHECK(!nd.accp.system_parameters_given, "and changes nothing");

    /* The bring-up sequence, in order. */
    body[0] = (uint8_t)NDBUS_ACCP_LSYSPAR;
    memset(&body[1], 0, 6);
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 7, replies);
    CHECK(n == 5 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "LSYSPAR is acked");
    CHECK(nd.accp.system_parameters_given, "and the guard cell is set");

    /* LPARP carries a 4-byte pointer, most significant byte first (T124). */
    body[0] = (uint8_t)NDBUS_ACCP_LPARP;
    body[1] = 0x00;
    body[2] = 0x00;
    body[3] = 0x08;
    body[4] = 0x40;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 5, replies);
    CHECK(n == 5 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "LPARP is acked");
    CHECK(nd.parameter_pointer == 0x00000840u, "the pointer is assembled MSB first");
    CHECK(nd.accp.parameter_pointer_given, "and the guard cell is set");

    /* STARTMIC, then the commands that are illegal while running. */
    body[0] = (uint8_t)NDBUS_ACCP_STARTMIC;
    body[1] = 0;
    body[2] = 0;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 3, replies);
    CHECK(n == 5 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "STARTMIC is acked");
    CHECK(nd.accp.microprogram_running, "the microprogram is running");

    body[0] = (uint8_t)NDBUS_ACCP_VPARP;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    /* A Messnak carries MFNACK, the error code and the ACCP status byte, so seven
     * frames: four of envelope and three of payload. MFNACK is 0xFF because the
     * reply's discriminator is the status HIGH byte - a raw error code there reads
     * as "no answer" rather than as a refusal. */
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 4 && payload[0] == 0xFF, "VPARP while running is answered with a Messnak");
    CHECK(plen == 4 && payload[1] == (uint8_t)NDBUS_ACCP_NAK_MICRO_RUNNING,
          "carrying the error code in its low byte");
    CHECK(nd.last_nak_code == NDBUS_ACCP_NAK_MICRO_RUNNING, "and the station recorded it");

    /* STOPMIC, and its inverse guard. */
    body[0] = (uint8_t)NDBUS_ACCP_STOPMIC;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    CHECK(n == 5 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "STOPMIC is acked while running");
    CHECK(!nd.accp.microprogram_running, "and the microprogram stops");
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    CHECK(nd.last_nak_code == NDBUS_ACCP_NAK_MICRO_NOT_STARTED,
          "a second STOPMIC naks 0 - the one inverse guard");

    /* CPURES clears the ACCP's own state and LEAVES SHARED MEMORY ALONE: the
     * ND-100 owns what is in the pool, and a reset that wiped it would destroy
     * the other side's data. */
    (void)ndbus_pool_write32(&pool, 0x600, 0x12345678u);
    body[0] = (uint8_t)NDBUS_ACCP_CPURES;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    CHECK(n == 5, "CPURES is answered");
    CHECK(!nd.accp.parameter_pointer_given, "the parameter pointer is forgotten");
    CHECK(!nd.accp.system_parameters_given, "the system parameters are forgotten");
    CHECK(ndbus_pool_read32(&pool, 0x600) == 0x12345678u, "and the shared pool is untouched");

    /* A SOMB for another OMD is the microprogram's message, not the ACCP's. */
    unsigned long handled_before = nd.messages_handled;
    uint16_t dest = (uint16_t)((uint16_t)NDBUS_STATION_ND5000_FIRST << NDBUS_FRAME_STATION_SHIFT);
    uint16_t somb0 = (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                                NDBUS_FRAME_S_STARTSTOP | 0u);
    (void)ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, somb0, replies);
    uint16_t eomb0 = (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE | 0u);
    (void)ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, eomb0, replies);
    CHECK(nd.messages_handled == handled_before, "a non-OMD-3 message is not the ACCP's");

    /* A new SOMB discards a half-collected message: the sender restarting is the
     * only way a second one can arrive. */
    body[0] = (uint8_t)NDBUS_ACCP_ECHO;
    body[1] = 1;
    body[2] = 0x11;
    (void)ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU,
                            (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                                       NDBUS_FRAME_S_STARTSTOP | (uint16_t)NDBUS_ACCP_OMD),
                            replies);
    (void)ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, (uint16_t)(dest | 0x99u), replies);
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 3, replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 2, "the restarted message is answered - Messack plus one echoed byte");
    CHECK(plen == 2 && payload[1] == 0x11,
          "and it echoes the SECOND message's byte, not the abandoned 0x99");
    CHECK(nd.last_command == NDBUS_ACCP_ECHO,
          "and it is the SECOND message, not the abandoned one");

    /* ---- the X5ACT doorbell ------------------------------------------------
     * The signature is the TRANSITION 0xFFFF -> 0. */
    ndbus_nd5000_reset(&nd);
    const uint32_t x5act = 0x700;
    (void)ndbus_pool_write16(&pool, x5act, 0xFFFF);

    CHECK(!ndbus_nd5000_sniff_before_write16(&nd, x5act, 0xFFFF),
          "writing -1 over -1 is no transition");
    CHECK(!ndbus_nd5000_sniff_before_write16(&nd, 0x710, 0),
          "a cell that was not -1 is not one either");
    CHECK(ndbus_nd5000_sniff_before_write16(&nd, x5act, 0),
          "the -1 to 0 write latches the doorbell");
    CHECK(nd.sniff.latched, "and the sniff is latched");
    CHECK(nd.sniff.candidate_offset == x5act, "at the right cell");

    /* THE THRESHOLD THAT CANNOT BE MET. XMSINIT sets X5ACT to -1 and the
     * microcode re-arms it to 1, so the genuine doorbell makes exactly ONE
     * -1 -> 0 transition per XMSINIT. A threshold of 2 can never be reached, the
     * sniff never latches, and nothing errors - the machine just looks idle. */
    ndbus_nd5000_reset(&nd);
    s_log_lines = 0;
    ndbus_nd5000_set_sniff_threshold(&nd, 2);
    CHECK(s_log_lines == 1, "setting an unreachable threshold warns at once");

    (void)ndbus_pool_write16(&pool, x5act, 0xFFFF);
    CHECK(!ndbus_nd5000_sniff_before_write16(&nd, x5act, 0), "the first transition does not latch");
    /* The re-arm writes 1, not -1 - so the next ring is 1 -> 0 and never matches
     * the signature again. This is the whole trap, in one check. */
    (void)ndbus_pool_write16(&pool, x5act, 1);
    CHECK(!ndbus_nd5000_sniff_before_write16(&nd, x5act, 0),
          "and the re-armed 1 to 0 ring does not match the signature");
    CHECK(!nd.sniff.latched, "so with a threshold of 2 the sniff NEVER latches");

    /* CALLED AFTER THE WRITE, THE SNIFF SEES NOTHING. The previous value it
     * reads is the new one, so no transition exists to find - and the failure is
     * silent, which is why the order is in the function's name. */
    ndbus_nd5000_reset(&nd);
    (void)ndbus_pool_write16(&pool, x5act, 0xFFFF);
    (void)ndbus_pool_write16(&pool, x5act, 0);   /* the write lands first */
    CHECK(!ndbus_nd5000_sniff_before_write16(&nd, x5act, 0),
          "sniffing AFTER the write finds no transition");
    CHECK(!nd.sniff.latched, "so the sniff never latches - the silent failure");

    /* 0 and 1 both mean latch on the first transition. */
    ndbus_nd5000_reset(&nd);
    ndbus_nd5000_set_sniff_threshold(&nd, 0);
    (void)ndbus_pool_write16(&pool, x5act, 0xFFFF);
    CHECK(ndbus_nd5000_sniff_before_write16(&nd, x5act, 0), "threshold 0 latches on the first");

    ndbus_pool_destroy(&pool);
}

/* ------------------------------------------------------------------------- */
/* Layer 3: the ND-100's word view of the byte-addressed pool                 */
/* ------------------------------------------------------------------------- */

static void test_window(void)
{
    printf("Layer 3: ND-100 window over the pool\n");

    NdbusPool pool;
    (void)ndbus_pool_create(&pool, POOL_BYTES);

    NdbusWindow win;
    /* A window that does not fit is REFUSED, not clamped: one that quietly ends
     * early hands the ND-100 memory reading as zero from some address on, which
     * looks like a guest bug rather than a configuration one. */
    CHECK(!ndbus_window_attach(&win, &pool, 0, POOL_BYTES), "a window twice the pool is refused");
    CHECK(!ndbus_window_attach(&win, &pool, POOL_BYTES - 8, 8), "and one that straddles the end");
    CHECK(!ndbus_window_attach(&win, NULL, 0, 16), "and one with no pool");
    CHECK(!ndbus_window_attach(&win, &pool, 0, 0), "and an empty one");

    const uint32_t base = 0x100;
    const uint32_t words = 64;
    CHECK(ndbus_window_attach(&win, &pool, base, words), "a window that fits attaches");

    /* WORD OFFSET TIMES TWO IS BYTE OFFSET, and the word is BIG-ENDIAN: byte 2N
     * is the HIGH half. Both rules in one check - if either were wrong, one of
     * these three would fail. */
    CHECK(ndbus_window_write_word(&win, 4, 0x1234), "word 4 writes");
    CHECK(ndbus_pool_read8(&pool, base + 8) == 0x12, "pool byte 2N is the HIGH byte");
    CHECK(ndbus_pool_read8(&pool, base + 9) == 0x34, "pool byte 2N+1 is the low byte");
    CHECK(ndbus_window_read_word(&win, 4) == 0x1234, "and it reads back");

    /* The reverse direction: what the ND-5000 writes as bytes, the ND-100 reads
     * as a word. This is the "one backing array, two ports" property, and it is
     * the whole reason the pool is shared rather than copied. */
    CHECK(ndbus_pool_write8(&pool, base + 20, 0xAB), "the ND-5000 writes a high byte");
    CHECK(ndbus_pool_write8(&pool, base + 21, 0xCD), "and a low byte");
    CHECK(ndbus_window_read_word(&win, 10) == 0xABCD, "the ND-100 reads them as one word");

    /* The window is an OFFSET INTO THE WINDOW, not into the pool: word 0 is at
     * the window's base, not at pool offset 0. */
    CHECK(ndbus_window_write_word(&win, 0, 0x7F7F), "word 0 writes");
    CHECK(ndbus_pool_read16(&pool, base) == 0x7F7F, "at the window base, not at pool 0");
    CHECK(ndbus_pool_read16(&pool, 0) == 0, "pool offset 0 is untouched");

    /* MSB is the HIGH byte - pool byte 2N, the FIRST of the pair. Writing the
     * wrong half leaves a value off by 256 and surfaces much later. */
    CHECK(ndbus_window_write_word(&win, 6, 0x0000), "clear word 6");
    CHECK(ndbus_window_write_msb(&win, 6, 0xEE), "MSB write");
    CHECK(ndbus_window_read_word(&win, 6) == 0xEE00, "MSB lands in the HIGH half");
    CHECK(ndbus_window_write_lsb(&win, 6, 0x11), "LSB write");
    CHECK(ndbus_window_read_word(&win, 6) == 0xEE11, "LSB lands in the low half");

    /* Outside the window. */
    CHECK(ndbus_window_read_word(&win, words) == 0, "one word past the end reads 0");
    CHECK(!ndbus_window_write_word(&win, words, 0xFFFF), "and refuses a write");
    CHECK(!ndbus_window_write_msb(&win, words, 0xFF), "so do the half-word writes");
    CHECK(!ndbus_window_write_lsb(&win, words, 0xFF), "both of them");
    /* The refused write must not have wrapped to the window base or anywhere
     * else in the pool. */
    CHECK(ndbus_window_read_word(&win, 0) == 0x7F7F, "the refused write went nowhere");

    /* The last word IS inside. An off-by-one here loses the top of the window. */
    CHECK(ndbus_window_write_word(&win, words - 1, 0x5A5A), "the last word is inside");
    CHECK(ndbus_pool_read16(&pool, base + (words - 1) * 2) == 0x5A5A, "at the right byte");

    ndbus_pool_destroy(&pool);
}

/* ------------------------------------------------------------------------- */
/* Layer 7: two mock CPUs on their own host threads                           */
/* ------------------------------------------------------------------------- */

#ifndef __EMSCRIPTEN__

typedef struct
{
    NdbusPool *pool;
    uint32_t   counter_offset;  /* this CPU's own counter, in the pool */
    uint32_t   shared_offset;   /* a counter both CPUs increment under TSET */
    uint32_t   semaphore;
    long       budget;          /* stop on its own after this many steps, or -1 */
    long       steps;
} MockCpu;

/* One "instruction": bump this CPU's own cell, then take the semaphore and bump
 * the shared cell. The shared one is the interesting part - if the lock cycle
 * were not atomic the total would come out short, exactly as in the TSET test,
 * but here it is two real host threads running an emulated CPU loop. */
static bool mock_step(void *ctx)
{
    MockCpu *cpu = (MockCpu *)ctx;
    cpu->steps++;

    uint32_t mine = ndbus_pool_read32(cpu->pool, cpu->counter_offset);
    (void)ndbus_pool_write32(cpu->pool, cpu->counter_offset, mine + 1);

    while (!ndbus_tset32(cpu->pool, cpu->semaphore, NULL))
    {
        /* spin */
    }
    uint32_t shared = ndbus_pool_read32(cpu->pool, cpu->shared_offset);
    (void)ndbus_pool_write32(cpu->pool, cpu->shared_offset, shared + 1);
    ndbus_semaphore_release32(cpu->pool, cpu->semaphore);

    if (cpu->budget >= 0 && cpu->steps >= cpu->budget)
    {
        return false; /* the CPU stops on its own - halted, not asked to */
    }
    return true;
}

static void test_runners(void)
{
    printf("Layer 7: two CPUs on host threads\n");

    NdbusPool pool;
    (void)ndbus_pool_create(&pool, POOL_BYTES);

    MockCpu a = {&pool, 0x800, 0x810, 0x820, -1, 0};
    MockCpu b = {&pool, 0x808, 0x810, 0x820, -1, 0};

    NdbusCpuOps ops_a;
    NdbusCpuOps ops_b;
    memset(&ops_a, 0, sizeof(ops_a));
    memset(&ops_b, 0, sizeof(ops_b));
    ops_a.name = "ND-5000 at 070B";
    ops_a.ctx = &a;
    ops_b.name = "ND-5000 at 071B";
    ops_b.ctx = &b;

    NdbusRunner run_a;
    NdbusRunner run_b;
    CHECK(ndbus_runner_init(&run_a, &ops_a, mock_step, NULL), "runner A initialises");
    CHECK(ndbus_runner_init(&run_b, &ops_b, mock_step, NULL), "runner B initialises");
    CHECK(ndbus_runner_state(&run_a) == NDBUS_RUNNER_IDLE, "and starts idle");

    /* A runner needs a CPU that can be stepped and a name to say so. */
    NdbusRunner bad;
    CHECK(!ndbus_runner_init(&bad, &ops_a, NULL, NULL), "a runner with no step is refused");
    NdbusCpuOps nameless;
    memset(&nameless, 0, sizeof(nameless));
    CHECK(!ndbus_runner_init(&bad, &nameless, mock_step, NULL), "and one with no name");

    CHECK(ndbus_runner_start(&run_a), "A starts");
    CHECK(ndbus_runner_start(&run_b), "B starts");
    CHECK(!ndbus_runner_start(&run_a), "starting A twice is refused");

    /* Let them run, then ask them to stop and WAIT. Requesting is not enough:
     * the thread may be mid-instruction when the request arrives, and anything
     * it touches must outlive it. */
    while (ndbus_runner_instructions(&run_a) < 2000ull ||
           ndbus_runner_instructions(&run_b) < 2000ull)
    {
        /* spin until both threads have made progress */
    }

    ndbus_runner_request_stop(&run_a);
    ndbus_runner_request_stop(&run_b);
    ndbus_runner_join(&run_a);
    ndbus_runner_join(&run_b);

    CHECK(ndbus_runner_state(&run_a) == NDBUS_RUNNER_IDLE, "A is idle again after the join");
    CHECK(ndbus_runner_state(&run_b) == NDBUS_RUNNER_IDLE, "so is B");
    CHECK(a.steps > 0 && b.steps > 0, "both CPUs actually ran");

    /* THE POINT. Each CPU's own counter matches its own step count - nothing
     * else touched it - and the shared counter equals the SUM, with no lost
     * update, because every increment went through the lock cycle. */
    CHECK(ndbus_pool_read32(&pool, 0x800) == (uint32_t)a.steps, "A's own counter is A's steps");
    CHECK(ndbus_pool_read32(&pool, 0x808) == (uint32_t)b.steps, "B's own counter is B's steps");
    CHECK(ndbus_pool_read32(&pool, 0x810) == (uint32_t)(a.steps + b.steps),
          "the shared counter is the SUM - no lost update across two threads");
    CHECK(ndbus_pool_read32(&pool, 0x820) == 0, "and the semaphore is free at the end");

    /* A CPU that stops on its own - halted, breakpoint, fault - ends the runner
     * without anyone asking. */
    MockCpu c = {&pool, 0x830, 0x838, 0x840, 500, 0};
    NdbusCpuOps ops_c;
    memset(&ops_c, 0, sizeof(ops_c));
    ops_c.name = "ND-5000 that halts";
    ops_c.ctx = &c;

    NdbusRunner run_c;
    CHECK(ndbus_runner_init(&run_c, &ops_c, mock_step, NULL), "the halting runner initialises");
    CHECK(ndbus_runner_start(&run_c), "and starts");
    ndbus_runner_join(&run_c); /* no stop request at all */
    CHECK(c.steps == 500, "it ran exactly its budget and stopped itself");
    CHECK(ndbus_runner_state(&run_c) == NDBUS_RUNNER_IDLE, "and the runner is idle");

    /* Joined, it can run again - the shutdown path is reusable, which is what a
     * restart needs. */
    c.steps = 0;
    CHECK(ndbus_runner_start(&run_c), "a joined runner starts again");
    ndbus_runner_stop_and_join(&run_c);
    CHECK(ndbus_runner_state(&run_c) == NDBUS_RUNNER_IDLE, "and stops cleanly");

    /* Stopping and joining something that never ran must be safe - a shutdown
     * path runs over every configured CPU, including ones that never started. */
    NdbusRunner never;
    CHECK(ndbus_runner_init(&never, &ops_a, mock_step, NULL), "a runner that never starts");
    ndbus_runner_stop_and_join(&never);
    ndbus_runner_stop_and_join(&never);
    CHECK(ndbus_runner_state(&never) == NDBUS_RUNNER_IDLE, "is safe to stop and join twice");
    ndbus_runner_request_stop(NULL);
    ndbus_runner_join(NULL);
    CHECK(ndbus_runner_instructions(NULL) == 0, "and NULL is safe everywhere");

    ndbus_pool_destroy(&pool);
}

#endif /* __EMSCRIPTEN__ */

/* ------------------------------------------------------------------------- */
/* Layer 9: the bring-up sequence, everything composed                        */
/* ------------------------------------------------------------------------- */

/*
 * The phase-4 exit condition in miniature: the ND-120 brings one ND-5000 up
 * over the octobus, with the shared pool underneath, and the CPU then runs.
 *
 * Every piece built so far is in this one path - pool, window, fabric, station,
 * multibyte reassembly, the ACCP guards, and a host thread - which is the only
 * way to find out whether they compose or merely each pass their own test.
 */
static void test_bringup(void)
{
    printf("Layer 9: ND-5000 bring-up, end to end\n");

    NdbusPool pool;
    (void)ndbus_pool_create(&pool, POOL_BYTES);

    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);

    NdbusNd5000 nd;
    CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, NULL, NULL),
          "the ND-5000 station comes up at 070B");
    CHECK(ndbus_fabric_register(&fabric, &nd.station), "and joins the bus");
    CHECK(ndbus_fabric_master(&fabric) == NDBUS_STATION_ND5000_FIRST,
          "with no ND-120 yet it is the MASTER - the lowest station on the bus");

    /* The ND-120 is station 1B and becomes MASTER the moment it appears. */
    NdbusStation nd120 = {NDBUS_STATION_ND120_CPU, "ND-120 CPU", NULL, NULL, NULL};
    CHECK(ndbus_fabric_register(&fabric, &nd120), "the ND-120 joins at 1B");
    CHECK(ndbus_fabric_master(&fabric) == NDBUS_STATION_ND120_CPU, "and takes over as MASTER");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
    uint8_t  body[8];

    /* 1. ECHO - prove the link carries bytes at all, before anything depends on
     *    it. The reply must be the pattern that was SENT. */
    body[0] = (uint8_t)NDBUS_ACCP_ECHO;
    body[1] = 3;
    body[2] = 0xDE;
    body[3] = 0xAD;
    body[4] = 0xBE;
    int n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 5,
                      replies);
    uint8_t payload[NDBUS_MAX_REPLY_FRAMES];
    int     plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                                      (int)sizeof(payload));
    CHECK(plen == 4, "ECHO answers with a Messack carrying three bytes");
    CHECK(plen == 4 && payload[0] == 0x00, "the Messack's leading status byte is MFACK");
    CHECK(plen == 4 && payload[1] == 0xDE, "echoing the pattern that was sent");
    CHECK(plen == 4 && payload[2] == 0xAD, "all of it");
    CHECK(plen == 4 && payload[3] == 0xBE, "in order");

    /* 2. LSYSPAR - where the microprogram sends octobus error messages. */
    body[0] = (uint8_t)NDBUS_ACCP_LSYSPAR;
    memset(&body[1], 0, 6);
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 7, replies);
    CHECK(n == 5 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "LSYSPAR is acked");

    /* 3. LPARP - where the parameter area lives in shared memory. */
    const uint32_t param_area = 0x400;
    body[0] = (uint8_t)NDBUS_ACCP_LPARP;
    body[1] = (uint8_t)(param_area >> 24);
    body[2] = (uint8_t)(param_area >> 16);
    body[3] = (uint8_t)(param_area >> 8);
    body[4] = (uint8_t)(param_area & 0xFF);
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 5, replies);
    CHECK(n == 5 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "LPARP is acked");
    CHECK(nd.parameter_pointer == param_area, "and the pointer is where the ND-120 said");

    /* 4. VPARP - THE check that the two agree. The ND-120 writes a word into the
     *    parameter area FIRST, and the ACCP must return THAT word, read out of
     *    shared memory. A canned Messack passes the guard and fails the check
     *    the command exists to perform. */
    (void)ndbus_pool_write32(&pool, param_area, 0xCAFEBABEu);
    body[0] = (uint8_t)NDBUS_ACCP_VPARP;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 5 && payload[0] == 0x00, "VPARP answers with a Messack plus four bytes");
    uint32_t echoed = ((uint32_t)payload[1] << 24) | ((uint32_t)payload[2] << 16) |
                      ((uint32_t)payload[3] << 8) | (uint32_t)payload[4];
    CHECK(echoed == 0xCAFEBABEu, "and it is the word the ND-120 put in SHARED MEMORY");

    /* Point the parameter area somewhere else and the answer changes - proof the
     * value came from the pool at the pointer rather than from a cache of the
     * last thing written. */
    (void)ndbus_pool_write32(&pool, 0x500, 0x12345678u);
    body[0] = (uint8_t)NDBUS_ACCP_LPARP;
    body[1] = 0;
    body[2] = 0;
    body[3] = 0x05;
    body[4] = 0x00;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 5, replies);
    body[0] = (uint8_t)NDBUS_ACCP_VPARP;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    plen   = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                                (int)sizeof(payload));
    echoed = (plen == 5) ? (((uint32_t)payload[1] << 24) | ((uint32_t)payload[2] << 16) |
                            ((uint32_t)payload[3] << 8) | (uint32_t)payload[4])
                         : 0u;
    CHECK(echoed == 0x12345678u, "a new parameter pointer reads a different word");

    /* 5. STARTMIC - the microprogram runs. The real start command is 066B; the
     *    manual's section order would have suggested 033B, which is RUNTST. */
    body[0] = (uint8_t)NDBUS_ACCP_STARTMIC;
    body[1] = 0;
    body[2] = 0;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 3, replies);
    CHECK(n == 5 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "STARTMIC is acked");
    CHECK(nd.accp.microprogram_running, "the microprogram is running");

    /* 6. And now the commands that load the control store are refused, because
     *    the microprogram is running - which is the state machine doing its job,
     *    not an error. */
    body[0] = (uint8_t)NDBUS_ACCP_VPARP;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    CHECK(nd.last_nak_code == NDBUS_ACCP_NAK_MICRO_RUNNING, "VPARP now naks -1");

#ifndef __EMSCRIPTEN__
    /* 7. With the CPU up, a host thread runs it against the SAME pool the ACCP
     *    just used - which is the whole architecture in one step: the octobus
     *    carried the commands, the shared memory carries the work. */
    MockCpu cpu = {&pool, 0x900, 0x908, 0x910, 1000, 0};
    NdbusCpuOps ops;
    memset(&ops, 0, sizeof(ops));
    ops.name = "ND-5000 at 070B";
    ops.ctx = &cpu;

    NdbusRunner runner;
    CHECK(ndbus_runner_init(&runner, &ops, mock_step, NULL), "the CPU gets a host thread");
    CHECK(ndbus_runner_start(&runner), "which starts");
    ndbus_runner_join(&runner);
    CHECK(cpu.steps == 1000, "and it ran");
    CHECK(ndbus_pool_read32(&pool, 0x900) == 1000u, "leaving its work in the shared pool");

    /* The ACCP's own state is untouched by the CPU having run - they are
     * different things sharing one machine. */
    CHECK(nd.accp.microprogram_running, "and the ACCP still says the microprogram is running");
#endif

    /* 8. STOPMIC, then CPURES: back to a cold ACCP, with SHARED MEMORY INTACT.
     *    The ND-100 owns what is in the pool; a CPU reset that wiped it would
     *    destroy the other side's data. */
    body[0] = (uint8_t)NDBUS_ACCP_STOPMIC;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    CHECK(n == 5 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "STOPMIC is acked");

    body[0] = (uint8_t)NDBUS_ACCP_CPURES;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    CHECK(n == 5, "CPURES is acked");
    CHECK(!nd.accp.parameter_pointer_given, "and the ACCP is cold again");
    CHECK(ndbus_pool_read32(&pool, param_area) == 0xCAFEBABEu, "shared memory survives the reset");
    CHECK(ndbus_pool_read32(&pool, 0x500) == 0x12345678u, "all of it");

    /* The bring-up can be run again from cold, which is what a restart is. */
    body[0] = (uint8_t)NDBUS_ACCP_LPARP;
    body[1] = 0;
    body[2] = 0;
    body[3] = 0x04;
    body[4] = 0x00;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 5, replies);
    CHECK(n == 5 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "and the sequence starts over");

    ndbus_pool_destroy(&pool);
}

/* ------------------------------------------------------------------------- */
/* Layer 6: the mailbox in shared memory                                      */
/* ------------------------------------------------------------------------- */

static void test_mailbox(void)
{
    printf("Layer 6: mailbox and the X5ACT doorbell\n");

    NdbusPool pool;
    (void)ndbus_pool_create(&pool, POOL_BYTES);

    NdbusMailbox mbx;
    const uint32_t header = 0x000;

    /* CPUNO IS 1-BASED. Slot 0 is the global header; a CPUNO of 0 would put the
     * extension block on top of it and overwrite X5SEM with a queue pointer. */
    CHECK(!ndbus_mailbox_attach(&mbx, &pool, header, 0), "CPUNO 0 is refused");
    CHECK(!ndbus_mailbox_attach(&mbx, &pool, header, -1), "and so is a negative one");
    CHECK(!ndbus_mailbox_attach(&mbx, &pool, header, NDBUS_MBX_MAX_CPUNO + 1),
          "and one past the last slot");
    CHECK(!ndbus_mailbox_attach(&mbx, NULL, header, 1), "and a view with no pool");

    CHECK(ndbus_mailbox_attach(&mbx, &pool, header, 1), "CPUNO 1 attaches");
    /* header + CPUNO * 256: the stride is 200B words = 128 words = 256 bytes. */
    CHECK(ndbus_mailbox_ext_base(&mbx) == header + 256u, "CPU 1's block is one stride in");

    NdbusMailbox mbx4;
    CHECK(ndbus_mailbox_attach(&mbx4, &pool, header, 4), "CPUNO 4 attaches");
    CHECK(ndbus_mailbox_ext_base(&mbx4) == header + 4u * 256u, "at four strides in");
    CHECK(ndbus_mailbox_ext_base(&mbx) != ndbus_mailbox_ext_base(&mbx4),
          "and two CPUs do not share an extension block");

    /* A mailbox that would run off the end is refused, not clamped: one that
     * does reads as zeros, which is indistinguishable from an empty queue. */
    NdbusMailbox off_end;
    CHECK(!ndbus_mailbox_attach(&off_end, &pool, POOL_BYTES - 256u, 1),
          "a mailbox that does not fit the pool is refused");

    /* The global header and the extension block are different memory. Writing
     * X5SEM must not disturb a CPU's queue head, and vice versa. */
    CHECK(ndbus_mailbox_write_global(&mbx, NDBUS_MBX_X5HEN_WORD, 0x1111), "X5HEN writes");
    CHECK(ndbus_mailbox_write_ext(&mbx, NDBUS_MBX_X5BEX_WORD, 0x2222), "X5BEX writes");
    CHECK(ndbus_mailbox_read_global(&mbx, NDBUS_MBX_X5HEN_WORD) == 0x1111, "X5HEN reads back");
    CHECK(ndbus_mailbox_read_ext(&mbx, NDBUS_MBX_X5BEX_WORD) == 0x2222, "X5BEX reads back");
    CHECK(ndbus_mailbox_read_global(&mbx, NDBUS_MBX_X5SEM_WORD) == 0, "X5SEM is untouched");

    /* The documented word offsets land where the ND documents say, in bytes. */
    CHECK(ndbus_pool_read16(&pool, header + 3u * 2u) == 0x1111, "X5HEN is global word 3");
    CHECK(ndbus_pool_read16(&pool, header + 256u) == 0x2222, "X5BEX is ext word 0");

    /* X5SEM goes through the same lock cycle as any other semaphore, so a guest
     * TSET on the same cell and the mailbox cannot both think they hold it. */
    CHECK(ndbus_mailbox_take_sem(&mbx, 0xFFFF), "X5SEM is taken");
    CHECK(!ndbus_mailbox_take_sem(&mbx, 0xFFFF), "and cannot be taken twice");
    CHECK(!ndbus_tset16(&pool, header + 0, 0x7000),
          "nor by a guest TSET on the same cell - one lock domain");
    ndbus_mailbox_release_sem(&mbx);
    CHECK(ndbus_mailbox_take_sem(&mbx, 0xFFFF), "released, it can be taken again");
    ndbus_mailbox_release_sem(&mbx);

    /* ---- THE DOORBELL, and why it is not symmetric ------------------------
     * XMSINIT sets X5ACT to -1. SINTRAN's ACT51 rings by writing 0, and sends
     * NO kick. The microcode's IDLE loop polls, and RE-ARMS WITH 1. */
    CHECK(ndbus_mailbox_write_ext(&mbx, NDBUS_MBX_X5ACT_WORD, NDBUS_MBX_X5ACT_INIT),
          "XMSINIT sets X5ACT to -1");
    CHECK(!ndbus_mailbox_poll(&mbx), "an unrung doorbell polls false");
    CHECK(ndbus_mailbox_read_ext(&mbx, NDBUS_MBX_X5ACT_WORD) == 0xFFFF,
          "and a poll that finds nothing changes nothing");

    CHECK(ndbus_mailbox_ring(&mbx), "ACT51 rings by writing 0");
    CHECK(ndbus_mailbox_read_ext(&mbx, NDBUS_MBX_X5ACT_WORD) == 0, "X5ACT is 0");
    CHECK(ndbus_mailbox_poll(&mbx), "the IDLE loop finds it");
    CHECK(ndbus_mailbox_read_ext(&mbx, NDBUS_MBX_X5ACT_WORD) == NDBUS_MBX_X5ACT_REARM,
          "and RE-ARMS WITH 1, not with -1");
    CHECK(NDBUS_MBX_X5ACT_REARM == 1u, "the re-arm value is 1");
    CHECK(NDBUS_MBX_X5ACT_REARM != 0xFFFFu, "and is NOT -1 - this is the whole trap");
    CHECK(!ndbus_mailbox_poll(&mbx), "a second poll finds nothing");

    /* So the 0xFFFF to 0 signature the self-discovery sniff keys on happens
     * exactly ONCE per XMSINIT. Every ring after the first re-arm is 1 to 0. */
    CHECK(ndbus_mailbox_ring(&mbx), "ringing again");
    CHECK(ndbus_mailbox_read_ext(&mbx, NDBUS_MBX_X5ACT_WORD) == 0, "sets X5ACT to 0 again");
    CHECK(ndbus_mailbox_poll(&mbx), "and the poll consumes it");
    /* The transition just seen was 1 -> 0, NOT 0xFFFF -> 0. That is why a sniff
     * threshold of 2 can never be met - see ndbus_nd5000.h. */

    /* One CPU's doorbell is not another's. */
    CHECK(ndbus_mailbox_write_ext(&mbx4, NDBUS_MBX_X5ACT_WORD, NDBUS_MBX_X5ACT_INIT),
          "CPU 4's doorbell is armed");
    CHECK(ndbus_mailbox_ring(&mbx), "ringing CPU 1");
    CHECK(ndbus_mailbox_read_ext(&mbx4, NDBUS_MBX_X5ACT_WORD) == 0xFFFF,
          "does not ring CPU 4");
    CHECK(!ndbus_mailbox_poll(&mbx4), "and CPU 4 polls false");

    /* ---- XMSINIT's picture ------------------------------------------------
     * The state the structure actually starts in, and it is NOT all -1 and not
     * all zero: X5SEM's free value is 0 while the three per-CPU cells are -1. */
    const uint32_t ring_byte = 0x00000800u;
    CHECK(ndbus_mailbox_init_xmsinit(&mbx, 32, ring_byte), "XMSINIT seeds the structure");
    CHECK(ndbus_mailbox_read_global(&mbx, NDBUS_MBX_X5SEM_WORD) == 0, "X5SEM free is 0");
    CHECK(ndbus_mailbox_read_global(&mbx, NDBUS_MBX_X5MXF_WORD) == 32, "X5MXF is the slot count");
    CHECK(ndbus_mailbox_read_ext(&mbx, NDBUS_MBX_X5BEX_WORD) == 0xFFFF, "X5BEX is -1, empty chain");
    CHECK(ndbus_mailbox_read_ext(&mbx, NDBUS_MBX_X5ACT_WORD) == 0xFFFF,
          "X5ACT is -1, nothing pending");
    CHECK(ndbus_mailbox_read_ext(&mbx, NDBUS_MBX_X5PRO_WORD) == 0xFFFF, "X5PRO is -1, idle");

    /* X5FIF IS A BYTE OFFSET, high word first - NOT a word address. The
     * microcode uses it directly as a byte address, so a word address puts
     * every ring slot at half its true offset: inside the structure rather than
     * outside it, which corrupts instead of faulting. */
    uint32_t fif = ((uint32_t)ndbus_mailbox_read_global(&mbx, NDBUS_MBX_X5FIF_WORD) << 16u) |
                   (uint32_t)ndbus_mailbox_read_global(&mbx, NDBUS_MBX_X5FIF_WORD + 1u);
    CHECK(fif == ring_byte, "X5FIF round-trips as a 32-bit value, high word first");
    CHECK(fif != (ring_byte >> 1u), "and is the BYTE offset, not the word address");

    /* Seeding one CPU must not disturb another that is already running. */
    CHECK(ndbus_mailbox_write_ext(&mbx4, NDBUS_MBX_X5ACT_WORD, 0x1234), "CPU 4 has its own state");
    CHECK(ndbus_mailbox_init_xmsinit(&mbx, 32, ring_byte), "CPU 1 is seeded again");
    CHECK(ndbus_mailbox_read_ext(&mbx4, NDBUS_MBX_X5ACT_WORD) == 0x1234, "CPU 4 is untouched");

    /* NULL is safe everywhere - a shutdown path walks every configured CPU. */
    CHECK(!ndbus_mailbox_init_xmsinit(NULL, 0, 0), "init(NULL) is refused");
    CHECK(ndbus_mailbox_ext_base(NULL) == 0, "ext_base(NULL) is 0");
    CHECK(!ndbus_mailbox_ring(NULL), "ring(NULL) is refused");
    CHECK(!ndbus_mailbox_poll(NULL), "poll(NULL) is refused");
    CHECK(!ndbus_mailbox_take_sem(NULL, 1), "take_sem(NULL) is refused");
    ndbus_mailbox_release_sem(NULL);
    CHECK(ndbus_mailbox_read_ext(NULL, 0) == 0, "read_ext(NULL) is 0");

    ndbus_pool_destroy(&pool);
}

/* ------------------------------------------------------------------------- */
/* Layer 5: the SAMSON context block                                          */
/* ------------------------------------------------------------------------- */

static void test_context(void)
{
    printf("Layer 5: SAMSON context block\n");

    NdbusPool pool;
    (void)ndbus_pool_create(&pool, POOL_BYTES);

    NdbusContext ctx;
    const uint32_t area = 0x000;

    CHECK(!ndbus_context_attach(&ctx, NULL, area, 0), "a view with no pool is refused");
    CHECK(!ndbus_context_attach(&ctx, &pool, area, -1), "a negative X5CPU is refused");
    CHECK(!ndbus_context_attach(&ctx, &pool, area, NDBUS_CTX_MAX_CPU + 1),
          "and one past the last CPU");
    CHECK(!ndbus_context_attach(&ctx, &pool, POOL_BYTES - 0x100, 0),
          "a block that runs off the end is refused, not clamped");

    CHECK(ndbus_context_attach(&ctx, &pool, area, 0), "X5CPU 0 attaches");
    /* area + 0x100 + 0x100 * X5CPU: the first CPU's block is ONE STRIDE IN,
     * the same 1-based shape the mailbox uses. */
    CHECK(ndbus_context_base(&ctx) == area + 0x100u, "CPU 0's block is one stride in");

    NdbusContext ctx3;
    CHECK(ndbus_context_attach(&ctx3, &pool, area, 3), "X5CPU 3 attaches");
    CHECK(ndbus_context_base(&ctx3) == area + 0x100u + 3u * 0x100u, "at four strides in");
    CHECK(ndbus_context_base(&ctx) != ndbus_context_base(&ctx3), "and CPUs do not share a block");

    /* The register file round-trips, 32-bit big-endian high halfword first. */
    CHECK(ndbus_context_write(&ctx, NDBUS_CTX_P, 0x00001000u), "P writes");
    CHECK(ndbus_context_write(&ctx, NDBUS_CTX_B, 0x00002000u), "B writes");
    CHECK(ndbus_context_read(&ctx, NDBUS_CTX_P) == 0x00001000u, "P reads back");
    CHECK(ndbus_context_read(&ctx, NDBUS_CTX_B) == 0x00002000u, "B reads back");
    CHECK(ndbus_pool_read16(&pool, ndbus_context_base(&ctx) + NDBUS_CTX_P) == 0x0000,
          "stored high halfword first");
    CHECK(ndbus_pool_read16(&pool, ndbus_context_base(&ctx) + NDBUS_CTX_P + 2) == 0x1000,
          "then the low halfword");

    /* The documented offsets, so a transcription slip shows up here rather than
     * as a CPU that starts with the wrong register in the wrong place. */
    CHECK(NDBUS_CTX_P == 0x00u && NDBUS_CTX_L == 0x04u, "P at 0x00, L at 0x04");
    CHECK(NDBUS_CTX_B == 0x08u && NDBUS_CTX_R == 0x0Cu, "B at 0x08, R at 0x0C");
    CHECK(NDBUS_CTX_I1 == 0x10u && NDBUS_CTX_I4 == 0x1Cu, "I1..I4 at 0x10..0x1C");
    CHECK(NDBUS_CTX_A1 == 0x20u && NDBUS_CTX_E4 == 0x3Cu, "A1..E4 at 0x20..0x3C");
    CHECK(NDBUS_CTX_STATUS == 0x40u, "status composite at 0x40");
    CHECK(NDBUS_CTX_CED == 0x5Cu && NDBUS_CTX_CAD == 0x60u, "CED at 0x5C, CAD at 0x60");

    /* THE TRAP: not every field in the block is loaded FROM the block. The
     * DOMAIN registers come from the Domain Information Table, and NEWCNTXT
     * does not touch them - so writing TOS here and expecting that stack
     * pointer does nothing, and nothing reports it. */
    CHECK(ndbus_context_field_is_loaded(NDBUS_CTX_P), "P is loaded from the block");
    CHECK(ndbus_context_field_is_loaded(NDBUS_CTX_B), "so is B");
    CHECK(ndbus_context_field_is_loaded(NDBUS_CTX_CED), "and CED");
    CHECK(!ndbus_context_field_is_loaded(NDBUS_CTX_DIT_TOS), "TOS is NOT - it is DIT-sourced");
    CHECK(!ndbus_context_field_is_loaded(NDBUS_CTX_DIT_LL), "nor LL, loaded by TRAPSET from DIT");
    CHECK(!ndbus_context_field_is_loaded(NDBUS_CTX_DIT_HL), "nor HL");
    CHECK(!ndbus_context_field_is_loaded(NDBUS_CTX_DIT_THA), "nor THA");
    CHECK(!ndbus_context_field_is_loaded(NDBUS_CTX_DIT_OTE1), "nor the trap enables");
    CHECK(!ndbus_context_field_is_loaded(NDBUS_CTX_DIT_TEM2), "any of them");

    /* THE STACK LIMITS ARE AN EMULATOR-PRIVATE STASH IN 0x4C AND 0x50, and the
     * two assertions above about them being DIT-sourced are about the HARDWARE,
     * which is a different claim. Both hold at once, so state the division here
     * or the next reader will take one of them for the whole truth.
     *
     * THE HARDWARE: the microcode loads neither slot from the block. The
     * whole-image sweep behind RetroCore's CpuND500.ProcessControl.cs walks every
     * AA=7 address word in all 16384 microwords and finds 0x4C and 0x50 touched
     * by NOTHING through the context base, in either direction.
     *
     * THE EMULATOR: because the machine never looks at them, nd100x's
     * mfbus_save_context/mfbus_load_context use them to carry TOS and LL across a
     * context switch. Writing a slot the machine ignores cannot mislead it;
     * leaving it unwritten while our own load reads it sets the stack limits to
     * zero on every switch. MEASURED 04-OCT-2026: the swapper stack-overflowed at
     * P=0x08008E09 right after a domain's page fault switched away from it,
     * faulting on 0x00000004 - a frame base of 4, not a missing page.
     *
     * WHY NOT THE DIT, which is where the hardware really keeps them: there is no
     * DIT to read on this lane. cpu->DITBASE is set to the CAPABILITY TABLE base,
     * 256 bytes per domain, and nd500_domain.h:47-73 warns that wiring
     * nd500_domain_load_state() up would write 16-byte-strided fields over a
     * guest's 256-byte-strided capability table and corrupt it silently. That was
     * tried first and the warning is why it was backed out.
     *
     * WHY HL AND THA ARE EXCLUDED, and this is the part a future change is most
     * likely to get wrong: the B30 save DOES write 0x54 and 0x58, and
     * MSG_UNIX5RE/MSG_UNIX5REL READ that pair while handling a mailbox message.
     * A value invented there is consumed as if SINTRAN had written it. 0x4C and
     * 0x50 are the only two slots in this group the microcode ignores outright,
     * which is the whole reason they are the only two used. */
    CHECK(NDBUS_CTX_DIT_TOS == 0x4Cu && NDBUS_CTX_DIT_LL == 0x50u,
          "the stash slots are 0x4C and 0x50 - the two the microcode ignores");
    CHECK(NDBUS_CTX_DIT_HL == 0x54u && NDBUS_CTX_DIT_THA == 0x58u,
          "HL and THA are 0x54/0x58 - the microcode READS these, so do not stash here");
    CHECK(ndbus_context_write(&ctx, NDBUS_CTX_DIT_TOS, 0x0001FFFCu) &&
          ndbus_context_read(&ctx, NDBUS_CTX_DIT_TOS) == 0x0001FFFCu,
          "TOS round-trips through the stash slot");
    CHECK(ndbus_context_write(&ctx, NDBUS_CTX_DIT_LL, 0x08026198u) &&
          ndbus_context_read(&ctx, NDBUS_CTX_DIT_LL) == 0x08026198u,
          "and LL does too - the swapper's own INIT value");

    /* Writing one is ACCEPTED - the cell exists - it simply has no effect on a
     * started CPU. The API does not pretend otherwise by refusing the write. */
    CHECK(ndbus_context_write(&ctx, NDBUS_CTX_DIT_TOS, 0xDEADBEEFu),
          "a DIT-sourced cell can still be written");
    CHECK(ndbus_context_read(&ctx, NDBUS_CTX_DIT_TOS) == 0xDEADBEEFu, "and read back");

    /* Placing a bring-up context clears the whole block first, so a previous
     * run's register file cannot leak into this one. */
    CHECK(ndbus_context_write(&ctx, NDBUS_CTX_I1, 0x11111111u), "leave a stale register");
    CHECK(ndbus_context_place(&ctx, 0x00003000u, 0x00004000u), "place a bring-up context");
    CHECK(ndbus_context_read(&ctx, NDBUS_CTX_P) == 0x00003000u, "P is the entry point");
    CHECK(ndbus_context_read(&ctx, NDBUS_CTX_B) == 0x00004000u, "B is the local data base");
    CHECK(ndbus_context_read(&ctx, NDBUS_CTX_I1) == 0, "and the stale register is cleared");
    CHECK(ndbus_context_read(&ctx, NDBUS_CTX_DIT_TOS) == 0, "along with everything else");

    /* Placing one CPU's context must not disturb another's. */
    CHECK(ndbus_context_place(&ctx3, 0x00005000u, 0x00006000u), "place CPU 3's context");
    CHECK(ndbus_context_read(&ctx, NDBUS_CTX_P) == 0x00003000u, "CPU 0's P is untouched");
    CHECK(ndbus_context_read(&ctx3, NDBUS_CTX_P) == 0x00005000u, "and CPU 3 has its own");

    /* ---- THE TWO CPU NUMBERINGS DO NOT MATCH -------------------------------
     * The mailbox extension block is indexed by a ONE-BASED CPUNO; the context
     * block area by a ZERO-BASED X5CPU. Both index a 256-byte stride off a base,
     * so the expressions look interchangeable and are not. Passing one where the
     * other belongs puts a CPU's registers in its neighbour's block, or its
     * queue head on top of the mailbox global header - and neither faults. */
    CHECK(ndbus_cpu_mailbox_cpuno(56) == 1, "station 070B is mailbox CPUNO 1");
    CHECK(ndbus_cpu_context_x5cpu(56) == 0, "and context X5CPU 0");
    CHECK(ndbus_cpu_mailbox_cpuno(56) != ndbus_cpu_context_x5cpu(56),
          "THE TWO DIFFER BY ONE for the same station - this is the trap");
    CHECK(ndbus_cpu_mailbox_cpuno(62) == 7, "station 076B is mailbox CPUNO 7");
    CHECK(ndbus_cpu_context_x5cpu(62) == 6, "and context X5CPU 6");

    /* Out of range gives a value each attach REFUSES, rather than silently
     * landing on block 0 or the global header. */
    CHECK(ndbus_cpu_mailbox_cpuno(1) == 0, "the ND-100's own station is not a CPUNO");
    CHECK(ndbus_cpu_context_x5cpu(1) == -1, "nor an X5CPU");
    CHECK(ndbus_cpu_mailbox_cpuno(8) == 0, "nor is a SCSI controller's station");

    /* And the refusal is real: the values are rejected by the attach calls. */
    {
        NdbusMailbox bad_mbx;
        NdbusContext bad_ctx;
        CHECK(!ndbus_mailbox_attach(&bad_mbx, &pool, 0, ndbus_cpu_mailbox_cpuno(1)),
              "a mailbox for a non-ND-5000 station is refused");
        CHECK(!ndbus_context_attach(&bad_ctx, &pool, 0, ndbus_cpu_context_x5cpu(1)),
              "so is a context block");
    }

    /* Same station, both structures, and they must land in DIFFERENT places. */
    {
        NdbusMailbox m70;
        NdbusContext c70;
        CHECK(ndbus_mailbox_attach(&m70, &pool, 0, ndbus_cpu_mailbox_cpuno(56)),
              "station 070B's mailbox attaches");
        CHECK(ndbus_context_attach(&c70, &pool, 0, ndbus_cpu_context_x5cpu(56)),
              "and its context block");
        /* Both are one stride in from their own base, which is WHY the two
         * numberings differ: the mailbox counts the header as slot 0, the
         * context area skips slot 0 with its +0x100. */
        CHECK(ndbus_mailbox_ext_base(&m70) == 256u, "the mailbox block is one stride in");
        CHECK(ndbus_context_base(&c70) == 0x100u, "and so is the context block");
    }

    /* Out of range, and NULL. */
    CHECK(!ndbus_context_write(&ctx, NDBUS_CTX_STRIDE_BYTES, 1), "past the block is refused");
    CHECK(ndbus_context_read(&ctx, NDBUS_CTX_STRIDE_BYTES) == 0, "and reads 0");
    CHECK(ndbus_context_base(NULL) == 0, "base(NULL) is 0");
    CHECK(!ndbus_context_place(NULL, 0, 0), "place(NULL) is refused");
    CHECK(ndbus_context_read(NULL, 0) == 0, "read(NULL) is 0");

    ndbus_pool_destroy(&pool);
}

/* ------------------------------------------------------------------------- */
/* Layer 7: the exactly-once canary                                           */
/* ------------------------------------------------------------------------- */

#ifndef __EMSCRIPTEN__

/*
 * RetroCore names the equivalent test
 * ThreadedCanary_KickAndX5Act_StopAnswerGiveintArrivesExactlyOnce, and the
 * property is the one most likely to be got wrong in a threaded design: a
 * message must be serviced EXACTLY ONCE - not lost, not twice - when two wake
 * paths (the X5ACT doorbell and a preempt kick) race.
 *
 * The distinction this test exists to pin down: RINGS ARE NOT MESSAGES. The
 * doorbell coalesces, deliberately, because the microcode's answer to a ring is
 * to walk the queue and find everything in it. So the assertion is NOT "one
 * service per ring" - that would be wrong on real hardware. It is "every queued
 * message is serviced exactly once, however the rings fall".
 *
 * The queue is the REAL X5BEX chain: message blocks in shared memory linked by
 * byte offset, walked the way the microcode walks it. Each block carries a
 * sequence number in a spare field so the drain can prove that every posted
 * block was taken exactly once - a counter would prove only that the totals
 * matched.
 */

#define CANARY_MESSAGES 20000

#define CANARY_BLOCK_BYTES 64u
#define CANARY_ARENA       0x1000u
/* The canary needs REAL distinct blocks - one per message - so it gets its own
 * pool rather than the 4 KB one the other layers use. 20000 blocks of 64 bytes
 * is 1.28 MB; 2 MB leaves room for the mailbox below the arena. The first
 * version of this test reused the small pool and its own arena-fits check caught
 * that, which is the check earning its place. */
#define CANARY_POOL_BYTES  (2u * 1024u * 1024u)

typedef struct
{
    NdbusMailbox *mbx;
    NdbusPool    *pool;
    unsigned char seen[CANARY_MESSAGES]; /* times each sequence number was taken */
    long          taken;
    /* Written by the ND-100 thread, read by the CPU thread. Touched only through
     * __atomic_* - `volatile` is NOT an atomic and a bare read/write pair here
     * is a C11 data race, which ThreadSanitizer duly reported when this test
     * first used one. The runner in ndbus_runner.c has always done it this way;
     * the harness had not. */
    unsigned      stop;
    long          polls;
    long          rings;
} Canary;

/* Walk the chain to exhaustion, recording each block's sequence number. This is
 * the microcode's answer to a ring, and it is what makes coalescing safe. */
static void canary_drain(Canary *c)
{
    uint32_t msg = 0;
    while (ndbus_msgqueue_take(c->mbx, &msg))
    {
        uint16_t seq = ndbus_msg_read(c->pool, msg, NDBUS_MSG_X5ACT);
        if (seq < CANARY_MESSAGES)
        {
            c->seen[seq]++;
        }
        c->taken++;
    }
}

/* The ND-5000 side: poll the doorbell and walk the chain. */
static void *canary_cpu(void *arg)
{
    Canary *c = (Canary *)arg;
    for (;;)
    {
        if (ndbus_mailbox_poll(c->mbx))
        {
            c->polls++;
            canary_drain(c);
        }

        if (__atomic_load_n(&c->stop, __ATOMIC_RELAXED) != 0u)
        {
            /* One last walk after the stop flag, so a block posted just before
             * it cannot be left on the chain. */
            canary_drain(c);
            return NULL;
        }
    }
}

static void test_exactly_once_canary(void)
{
    printf("Layer 7: exactly-once canary\n");

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, CANARY_POOL_BYTES), "a pool big enough for real blocks");

    NdbusMailbox mbx;
    CHECK(ndbus_mailbox_attach(&mbx, &pool, 0, 1), "a mailbox for CPU 1");
    CHECK(ndbus_mailbox_init_xmsinit(&mbx, 32, 0x800), "seeded as XMSINIT leaves it");

    /* An arena of message blocks, one per message, each with its sequence
     * number in X5ACT. Blocks are distinct memory so a double-take is provable
     * rather than inferred from a total. */
    static Canary c;
    memset(&c, 0, sizeof(c));
    c.mbx = &mbx;
    c.pool = &pool;

    CHECK(CANARY_ARENA + (uint32_t)CANARY_MESSAGES * CANARY_BLOCK_BYTES <= CANARY_POOL_BYTES,
          "the block arena fits the pool");

    pthread_t cpu;
    if (pthread_create(&cpu, NULL, canary_cpu, &c) != 0)
    {
        printf("  SKIP: could not create the CPU thread\n");
        ndbus_pool_destroy(&pool);
        return;
    }

    /* The ND-100 side: build a block, post it onto the chain, then ring. In
     * that order - the release in the ring publishes everything before it. */
    for (int i = 0; i < CANARY_MESSAGES; i++)
    {
        uint32_t msg = CANARY_ARENA + (uint32_t)i * CANARY_BLOCK_BYTES;
        (void)ndbus_msg_write(&pool, msg, NDBUS_MSG_X5ACT, (uint16_t)i);
        (void)ndbus_msg_write(&pool, msg, NDBUS_MSG_SENDE, 1u);
        (void)ndbus_msgqueue_post(&mbx, msg);

        (void)ndbus_mailbox_ring(&mbx);
        c.rings++;
    }

    __atomic_store_n(&c.stop, 1u, __ATOMIC_RELAXED);
    (void)pthread_join(cpu, NULL);

    long posted = CANARY_MESSAGES;
    long taken = c.taken;

    /* THE CANARY. Not one service per ring - one service per MESSAGE, and
     * proved per block rather than by a total: a total would pass if one block
     * were taken twice and another lost. */
    int missing = 0;
    int doubled = 0;
    for (int i = 0; i < CANARY_MESSAGES; i++)
    {
        if (c.seen[i] == 0)
        {
            missing++;
        }
        else if (c.seen[i] > 1)
        {
            doubled++;
        }
    }
    CHECK(missing == 0, "no queued message was lost");
    CHECK(doubled == 0, "and none was serviced twice");
    CHECK(taken == posted, "so the totals agree too");
    CHECK(ndbus_msgqueue_depth(&mbx, CANARY_MESSAGES + 1) == 0, "the chain is empty at the end");

    /* And the proof that rings really do coalesce, so the assertion above is
     * the meaningful one: far fewer polls succeeded than rings were sent, yet
     * nothing was lost. If these were equal the test would not be exercising
     * coalescing at all. */
    CHECK(c.rings == CANARY_MESSAGES, "the ND-100 rang once per message");
    printf("  rings=%ld, successful polls=%ld (coalescing is the difference)\n", c.rings,
           c.polls);
    CHECK(c.polls <= c.rings, "polls never exceed rings - no service was invented");

    /* THE DOORBELL'S STATE IS NOT THE WORK'S STATE, and this is where that
     * shows. The last ring can land after the CPU's final poll, leaving X5ACT
     * at 0 - rung, unpolled - while the queue is nevertheless fully drained.
     *
     * That is harmless and it is the point: work lives in the queue, the
     * doorbell only says to go and look. A design that inferred "work pending"
     * from a rung doorbell would report outstanding work here and be wrong, and
     * one that inferred "no work" from a re-armed doorbell would be wrong the
     * other way. The first version of this check asserted the doorbell was left
     * re-armed and failed for exactly that reason. */
    uint16_t x5act_at_end = ndbus_mailbox_read_ext(&mbx, NDBUS_MBX_X5ACT_WORD);
    CHECK(ndbus_msgqueue_depth(&mbx, 4) == 0, "the chain is drained whatever the doorbell says");
    CHECK(x5act_at_end == 0 || x5act_at_end == NDBUS_MBX_X5ACT_REARM,
          "and the doorbell is in one of its two legal states, rung or re-armed");

    /* A leftover ring costs nothing: the next poll consumes it and finds an
     * empty queue, which is a no-op rather than a spurious service. */
    if (x5act_at_end == 0)
    {
        CHECK(ndbus_mailbox_poll(&mbx), "a leftover ring is consumed by the next poll");
        CHECK(ndbus_msgqueue_depth(&mbx, 4) == 0,
              "and finds nothing, because the chain is already empty");
    }

    ndbus_pool_destroy(&pool);
}

#endif /* __EMSCRIPTEN__ */


/*
 * Layer 10: THE FRAMES SINTRAN ACTUALLY SENT.
 *
 * Every other test in this file was written from our own reading of the wire
 * format, and every one of them passed while the station read the command byte two
 * bytes too early, answered with an unterminated message, and put station 0 in the
 * source field. A test written from the same reading as the code cannot catch the
 * reading being wrong. So this one is written from a CAPTURE.
 *
 * Captured 26-SEP-2026 from SINTRAN III VSX/500 L booting on nd100x, driving the
 * real ND-500/5000 MONITOR J04 - the IOX writes to the octobus card's command
 * register 100405, in order, exactly as the trace printed them:
 *
 *   134063   SOMB  station 070B  omd 3
 *   034003   data  03   source OMD
 *   034007   data  07   payload byte count
 *   034016   data  0E   COMMAND 016B - LSYSPAR
 *   034001   data  01
 *   034003   data  03
 *   034000   data  00
 *   034000   data  00
 *   034000   data  00
 *   034000   data  00
 *   134043   EOMB  station 070B  omd 3
 *
 * and, at cold start, CH5CPUPRESENT's CPU probe to each station:
 *
 *   134241   emergency 241B  CMMACLE  master clear
 *   134242   emergency 242B  CMACONT  continue ACCP
 *
 * The frames go in as 16-bit words with no interpretation. If the station's
 * reading of them ever drifts again, this fails.
 */
static void test_captured_sintran_frames(void)
{
    printf("Layer 10: the frames SINTRAN actually sent\n");

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the station to sit on");

    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);

    NdbusNd5000 nd;
    CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, NULL, NULL),
          "a station at 070B, where SINTRAN addressed one");
    CHECK(ndbus_fabric_register(&fabric, &nd.station), "registered on the fabric");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];

    /* ---- cold start: CH5CPUPRESENT's two emergency frames ---- */

    nd.accp.microprogram_running = true; /* so the master clear has something to clear */
    nd.accp_idle                 = true; /* and the continue has something to leave */

    int n = ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, 0134241u, replies);
    CHECK(n == 0, "241B master clear is acknowledged with no reply frame");
    CHECK(nd.master_clears == 1, "and the station handled it as a master clear");
    CHECK(nd.last_emergency == 0xA1u, "the emergency code is the WHOLE low byte, 241B");
    CHECK(!nd.accp.microprogram_running, "the master clear stopped the microprogram");
    CHECK(!nd.accp_idle, "and left the idle loop");

    nd.accp_idle = true;
    n            = ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, 0134242u, replies);
    CHECK(n == 0, "242B continue ACCP is acknowledged with no reply frame");
    CHECK(nd.continues == 1, "and the station handled it as a continue");
    CHECK(nd.last_emergency == 0xA2u, "code 242B, which does not fit in four bits");
    CHECK(!nd.accp_idle, "the ACCP is out of its idle loop");

    /* 244B is the ND-500 monitor's timeout terminate, the third of the set. */
    n = ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, 0134244u, replies);
    CHECK(n == 0 && nd.terminates == 1, "244B terminate ACCP is handled too");
    CHECK(nd.accp_idle && !nd.accp.microprogram_running,
          "it enters the idle loop and stops the microprogram");

    /* Back to a live ACCP for the message below. */
    (void)ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, 0134241u, replies);
    (void)ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, 0134242u, replies);

    /* ---- the monitor's message, frame by captured frame ---- */

    static const uint16_t captured[] = {
        0134063u, /* SOMB omd 3 */
        0034003u, /* source OMD 3 */
        0034007u, /* count 7 */
        0034016u, /* command 016B */
        0034001u, 0034003u, 0034000u, 0034000u, 0034000u, 0034000u,
        0134043u, /* EOMB omd 3 */
    };

    n = 0;
    for (size_t i = 0; i < sizeof(captured) / sizeof(captured[0]); i++)
    {
        n = ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU, captured[i], replies);
    }

    CHECK(nd.messages_handled == 1, "the station assembled one complete message");
    CHECK(nd.last_command == 0x0Eu,
          "and read the command from byte 2 of the body: 016B, not the 003B at byte 0");
    CHECK(nd.last_nak_code == NDBUS_ACCP_ACCEPTED,
          "016B is LSYSPAR, which the station implements - so it is ACCEPTED, not naked 6");
    CHECK(nd.accp.system_parameters_given, "and the guard cell it sets is set");

    /* THE REPLY IS A WHOLE MESSAGE FROM A REAL STATION. Before the fix this was one
     * frame, 0100046 - a bare control frame with the nak code in the CODE field,
     * which on the wire reads as "EOMB, OMD 6" from station 0. */
    uint8_t payload[NDBUS_MAX_REPLY_FRAMES];
    int     plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                                      (int)sizeof(payload));
    /* ONE payload byte: the real ND-324716 firmware acks with a single 0x00
     * (measured 2026-09-18; RetroCore's station sends this shape by default). */
    CHECK(plen == 1, "the reply is a properly enveloped Messack");
    CHECK(plen == 1 && payload[0] == 0x00,
          "carrying the single zero status byte that means OK");
    CHECK(n >= 1 && ndbus_frame_station(replies[0]) == NDBUS_STATION_ND5000_FIRST,
          "and every frame says it came from 070B, not from station 0");
    CHECK(n >= 1 && (replies[0] & NDBUS_FRAME_CODE_MASK) == 3u,
          "addressed to the source OMD the message named, 3");
}


/* ------------------------------------------------------------------------- */
/* Layer 11: the OMD-0 Octobus Test Protocol                                  */
/* ------------------------------------------------------------------------- */

/*
 * THE REQUESTS IN THIS SECTION ARE THE RECORDED ONES, byte for byte, and that is
 * deliberate. The bodies below were captured from a live TPE OCTOBUS B00 run and
 * are quoted in RetroCore
 * Emulated.Tests.ND100/ControllerOctobus/OctobusTpeConfigReproTests.cs:
 *
 *     identify yourself   00 04 71 C7 00 00
 *     echo single word    00 08 71 C7 00 0C 00 01 FF FF
 *
 * and the reply BYTE COUNTS asserted here are TPE's own: 8 for identify, 132 for
 * get-present-stations, 12 for echo-single. Writing the expected bytes out of a
 * reading of the protocol instead would let a test agree with a mistaken
 * implementation, which is how the "command byte is body[0]" bug survived.
 */

/* Send one OMD-0 Test Protocol request the way TPE's
 * octobus_send_multibyte_message does: SOMB to OMD 0, the source OMD byte, the
 * byte count, one data frame per body byte, EOMB. `body` is the WHOLE collected
 * message INCLUDING its first two header bytes, exactly as recorded above, so the
 * test cannot silently disagree with the capture about what the header is. */
static int send_omd0(NdbusFabric *fabric, uint8_t from, uint8_t to, const uint8_t *body,
                     int length, uint16_t *replies)
{
    uint16_t dest = (uint16_t)((uint16_t)to << NDBUS_FRAME_STATION_SHIFT);

    uint16_t somb = (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                               NDBUS_FRAME_S_STARTSTOP | (uint16_t)NDBUS_TESTPROTO_OMD);
    (void)ndbus_fabric_send(fabric, from, somb, replies);

    for (int i = 0; i < length; i++)
    {
        (void)ndbus_fabric_send(fabric, from, (uint16_t)(dest | (uint16_t)body[i]), replies);
    }

    uint16_t eomb = (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                               (uint16_t)NDBUS_TESTPROTO_OMD);
    return ndbus_fabric_send(fabric, from, eomb, replies);
}

/* One payload word, big-endian, out of an unwrapped reply. */
static uint16_t reply_word(const uint8_t *payload, int word_index)
{
    return (uint16_t)(((uint16_t)payload[word_index * 2] << 8) |
                      (uint16_t)payload[(word_index * 2) + 1]);
}

/*
 * Layer 12: the control-store load, from the captured monitor exchange.
 *
 * EVERY NUMBER HERE WAS MEASURED, not chosen. Live run 30-SEP-2026, nd100x with
 * ND5000.ini, SINTRAN III VSX/500 L, ND-500/5000 MONITOR J04, command
 * START-SWAPPER, with every OMD-3 command logged as it reached the station:
 *
 *     LPARP  pointer 0x00000800
 *     VPARP  (the pattern 6596 9B49 lies at the pointer)
 *     LOCSM  x 128: parameter word0 = 0x0080, word1 = 0x0000, 0x0080 ... 0x3F80
 *     DUCS   x 1:   parameter word0 = 0x0001, word1 = 0x0000
 *
 * and the monitor then printed "ACCP command status: Checksum error". Before the
 * fix nothing moved: the eight halfwords at the pointer were the last LOCSM
 * pulse's own header and payload, 0001 0000 0040 0000 0001 8000 0000 0000, summing
 * to 0x8042, against an addend word of 0x0080 - N left over from that pulse. Both
 * of those values are asserted below as the FAILING case, so this test cannot
 * silently start agreeing with a broken implementation the way the mfbus_bridge
 * MSB test once did.
 */
static void test_control_store_load(void)
{
    printf("Layer 12: the ND-5000 control-store load and its checksum\n");

    NdbusPool pool;
    /* 256 KB, because the mailbox base the monitor patches in lands at pool
     * offset 0x8800 and the out-of-window guard rightly refuses a base whose whole
     * 256-byte extension block does not fit. */
    CHECK(ndbus_pool_create(&pool, 256 * 1024), "a pool for the control-store load");

    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);

    NdbusNd5000 nd;
    CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, NULL, NULL),
          "a station at 070B, where the monitor addressed one");
    CHECK(ndbus_fabric_register(&fabric, &nd.station), "registered on the fabric");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
    uint8_t  body[8];

    /* The pointer the monitor used. LPARP carries it most significant byte
     * first (T124). */
    const uint32_t pb = 0x00000800u;
    body[0]           = (uint8_t)NDBUS_ACCP_LPARP;
    body[1]           = (uint8_t)(pb >> 24u);
    body[2]           = (uint8_t)(pb >> 16u);
    body[3]           = (uint8_t)(pb >> 8u);
    body[4]           = (uint8_t)(pb & 0xFFu);
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 5,
                    replies);
    CHECK(nd.parameter_pointer == pb, "LPARP took the pointer the monitor sent");

    /* ONE LOCSM PULSE, N = 1, at control-store address 0 - the address the single
     * DUCS pulse reads back. The eight halfwords are the monitor's own first
     * microword, from the capture's first LOCSM line at pb+4:
     *     0000 0000 0001 8000 0000 0000 194F 2E9A */
    static const uint16_t micro0[8] = { 0x0000u, 0x0000u, 0x0001u, 0x8000u,
                                        0x0000u, 0x0000u, 0x194Fu, 0x2E9Au };
    (void)ndbus_pool_write16(&pool, pb, 1u);      /* word0 = N */
    (void)ndbus_pool_write16(&pool, pb + 2u, 0u); /* word1 = control-store address */
    for (uint32_t i = 0u; i < 8u; i++)
    {
        (void)ndbus_pool_write16(&pool, pb + 4u + (i * 2u), micro0[i]);
    }

    body[0] = (uint8_t)NDBUS_ACCP_LOCSM;
    int n   = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                        replies);
    CHECK(n > 0, "LOCSM is answered");
    CHECK(nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "and answered with a Messack");
    CHECK(nd.cs_load_pulses == 1, "the station serviced the pulse instead of ignoring it");

    /* THE READ-BACK. Parameter word0 = N = 1, word1 = 0, exactly as captured. */
    (void)ndbus_pool_write16(&pool, pb, 1u);
    (void)ndbus_pool_write16(&pool, pb + 2u, 0u);

    body[0] = (uint8_t)NDBUS_ACCP_DUCS;
    n       = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                        replies);
    CHECK(n > 0, "DUCS is answered");
    CHECK(nd.cs_dump_pulses == 1, "and the read-back was served");

    /* The dump is the microword, verbatim, AT the pointer. */
    bool same = true;
    for (uint32_t i = 0u; i < 8u; i++)
    {
        if (ndbus_pool_read16(&pool, pb + (i * 2u)) != micro0[i])
        {
            same = false;
        }
    }
    CHECK(same, "the read-back serves the eight halfwords the load pulse wrote");

    /* And the addend is their 16-bit wrapping sum, in the halfword straight after
     * them - manual ND-05.020.01 sec 5.3.20, "the dumped N x (8 x 16b) microwords
     * + checksum addend". 0x0001 + 0x8000 + 0x194F + 0x2E9A = 0xC1EA. */
    uint16_t expected = 0u;
    for (uint32_t i = 0u; i < 8u; i++)
    {
        expected = (uint16_t)((expected + micro0[i]) & 0xFFFFu);
    }
    CHECK(expected == 0xC7EAu, "the sum of the captured microword is 0xC7EA");
    CHECK(ndbus_pool_read16(&pool, pb + 16u) == expected,
          "the addend word after the dump is that sum - what the monitor compares against");
    CHECK(nd.cs_last_addend == expected, "and the station reports the same addend");

    /* THE FAILING CASE THIS REPLACES. Before the fix the halfwords at the pointer
     * were the last pulse's leftovers, starting with the DUCS parameter word
     * itself; a serviced read-back overwrites them. */
    CHECK(ndbus_pool_read16(&pool, pb) == micro0[0],
          "the dump overwrote the DUCS parameter word instead of leaving it to be summed");

    /* A SECOND control-store address must read back its OWN microword. Without
     * this the test above passes for an implementation that always serves
     * microword 0. */
    static const uint16_t micro1[8] = { 0x0080u, 0x0000u, 0x0040u, 0x0000u,
                                        0x0001u, 0x8000u, 0x0000u, 0x0000u };
    (void)ndbus_pool_write16(&pool, pb, 1u);
    (void)ndbus_pool_write16(&pool, pb + 2u, 0x3F80u); /* the last address the monitor loaded */
    for (uint32_t i = 0u; i < 8u; i++)
    {
        (void)ndbus_pool_write16(&pool, pb + 4u + (i * 2u), micro1[i]);
    }
    body[0] = (uint8_t)NDBUS_ACCP_LOCSM;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                    replies);

    (void)ndbus_pool_write16(&pool, pb, 1u);
    (void)ndbus_pool_write16(&pool, pb + 2u, 0x3F80u);
    body[0] = (uint8_t)NDBUS_ACCP_DUCS;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                    replies);

    same = true;
    for (uint32_t i = 0u; i < 8u; i++)
    {
        if (ndbus_pool_read16(&pool, pb + (i * 2u)) != micro1[i])
        {
            same = false;
        }
    }
    CHECK(same, "control-store address 3F80B reads back its own microword, not word 0's");

    /* An N greater than one fans out to consecutive control-store addresses, which
     * is how the real load works: 128 microwords per pulse. */
    (void)ndbus_pool_write16(&pool, pb, 2u);
    (void)ndbus_pool_write16(&pool, pb + 2u, 0x0100u);
    for (uint32_t i = 0u; i < 16u; i++)
    {
        (void)ndbus_pool_write16(&pool, pb + 4u + (i * 2u), (uint16_t)(0xA000u + i));
    }
    body[0] = (uint8_t)NDBUS_ACCP_LOCSM;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                    replies);

    (void)ndbus_pool_write16(&pool, pb, 1u);
    (void)ndbus_pool_write16(&pool, pb + 2u, 0x0101u); /* the SECOND microword of that pulse */
    body[0] = (uint8_t)NDBUS_ACCP_DUCS;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                    replies);
    same = true;
    for (uint32_t i = 0u; i < 8u; i++)
    {
        if (ndbus_pool_read16(&pool, pb + (i * 2u)) != (uint16_t)(0xA008u + i))
        {
            same = false;
        }
    }
    CHECK(same, "a pulse of N=2 loads the second microword at the next address");

    /* ---- ENKICK finds the mailbox in the control store ----
     *
     * MEASURED at ENKICK on the live run of 30-SEP-2026, read out of this
     * station's own control store:
     *     cs[0x16] = 4000 0001 DE01 6010 0000 0000 0000 8800  START_MESS = 0x8800
     *     cs[0x15] = 4000 0001 DE01 6010 0000 0000 0000 0001  SAMSON_CPU = 1
     * which is the pair RetroCore's OctobusCsLoadTests
     * CsLoad_Enkick_DerivesMailboxBaseFromStartMess pins. Both cells are LARG
     * constants, so halfwords 6 and 7 together are the 32-bit value. */
    static const uint16_t cs15[8] = { 0x4000u, 0x0001u, 0xDE01u, 0x6010u,
                                      0x0000u, 0x0000u, 0x0000u, 0x0001u };
    static const uint16_t cs16[8] = { 0x4000u, 0x0001u, 0xDE01u, 0x6010u,
                                      0x0000u, 0x0000u, 0x0000u, 0x8800u };
    (void)ndbus_pool_write16(&pool, pb, 2u);           /* two microwords ... */
    (void)ndbus_pool_write16(&pool, pb + 2u, 0x15u);   /* ... at 025B and 026B */
    for (uint32_t i = 0u; i < 8u; i++)
    {
        (void)ndbus_pool_write16(&pool, pb + 4u + (i * 2u), cs15[i]);
        (void)ndbus_pool_write16(&pool, pb + 20u + (i * 2u), cs16[i]);
    }
    body[0] = (uint8_t)NDBUS_ACCP_LOCSM;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                    replies);

    /* ENKICK is the ACCP-to-microprogram handoff, and the mailbox must be known by
     * the time it returns. */
    body[0] = (uint8_t)NDBUS_ACCP_ENKICK;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                    replies);

    CHECK(nd.start_mess == 0x8800u, "ENKICK read START_MESS 0x8800 out of word 026B");
    CHECK(nd.samson_cpu == 1u, "and SAMSON_CPU 1 out of word 025B");
    CHECK(ndbus_mailbox_ext_base(&nd.mailbox) == 0x8800u + 256u,
          "the extension block is the header plus SAMSON_CPU strides of 256 bytes");
    CHECK(nd.sniff.latched,
          "and the 0xFFFF->0 doorbell sniff is stood down - the CS is the answer");

    /* A truncating read of halfword 7 alone would give the same answer for this
     * pair, so prove the HIGH half is read too: a base past 0xFFFF must survive. */
    (void)ndbus_pool_write16(&pool, pb, 1u);
    (void)ndbus_pool_write16(&pool, pb + 2u, 0x16u);
    for (uint32_t i = 0u; i < 8u; i++)
    {
        (void)ndbus_pool_write16(&pool, pb + 4u + (i * 2u), cs16[i]);
    }
    (void)ndbus_pool_write16(&pool, pb + 4u + (6u * 2u), 0x0001u); /* halfword 6 = high half */
    body[0] = (uint8_t)NDBUS_ACCP_LOCSM;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                    replies);
    body[0] = (uint8_t)NDBUS_ACCP_ENKICK;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                    replies);
    CHECK(nd.start_mess == 0x00018800u,
          "START_MESS is the 32-bit LARG field, not halfword 7 truncated to 16 bits");

    /* LOCSM with no parameter pointer is refused by the guard table, so nothing is
     * allocated and nothing is copied - a garbage staging block at offset 0 must
     * not reach the control store. */
    NdbusNd5000 cold;
    CHECK(ndbus_nd5000_init(&cold, NDBUS_STATION_ND5000_LAST, &pool, NULL, NULL),
          "a second station, cold");
    CHECK(ndbus_fabric_register(&fabric, &cold.station), "registered at 076B");
    body[0] = (uint8_t)NDBUS_ACCP_LOCSM;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_LAST, body, 1,
                    replies);
    CHECK(cold.last_nak_code == NDBUS_ACCP_NAK_NO_PARAM_POINTER,
          "LOCSM without LPARP is Messnak 1, not a load from offset zero");
    CHECK(cold.cs_load_pulses == 0, "and no pulse was serviced");
    CHECK(cold.control_store == NULL, "and no control store was allocated");

    ndbus_nd5000_destroy(&cold);
    ndbus_nd5000_destroy(&nd);
    ndbus_pool_destroy(&pool);
}

static void test_test_protocol(void)
{
    printf("Layer 11: the OMD-0 octobus test protocol\n");

    NdbusPool pool;
    (void)ndbus_pool_create(&pool, POOL_BYTES);

    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);

    /* Station 1B, the ND-100 card, is the asker. It is registered so the
     * get-present-stations reply has something true to say about it; it is silent
     * because nothing here sends it a frame. */
    NdbusStation nd120 = {NDBUS_STATION_ND120_CPU, "ND-120 CPU", NULL, NULL, NULL};
    CHECK(ndbus_fabric_register(&fabric, &nd120), "the ND-100 card registers as 1B");

    NdbusNd5000 nd;
    CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, NULL, NULL),
          "the ND-5000 comes up at 070B");
    CHECK(ndbus_fabric_register(&fabric, &nd.station), "and registers on the fabric");
    CHECK(nd.station.fabric == &fabric,
          "registering hands the station its fabric - the get-present reply needs the registry");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
    uint8_t  payload[NDBUS_MULTIBYTE_MAX];
    int      n;
    int      plen;

    /* ---- identify yourself: the recorded body, and an 8-byte reply ---------- */
    const uint8_t identify[] = {0x00u, 0x04u, 0x71u, 0xC7u, 0x00u, 0x00u};
    n    = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, identify,
                     (int)sizeof(identify), replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 8, "identify yourself is answered with 8 payload bytes - header only");
    CHECK(plen == 8 && reply_word(payload, 0) == 0x71C7u, "reply word0 is the 0x71C7 magic");
    CHECK(plen == 8 && reply_word(payload, 1) == 0x0001u, "reply word1 is the command plus one");
    CHECK(plen == 8 && reply_word(payload, 2) == NDBUS_STATION_ND5000_FIRST,
          "reply word2 is the answering station, 56 decimal = 070B");
    CHECK(plen == 8 && reply_word(payload, 3) == 0u, "reply word3 is status 0, Ok");
    CHECK(n >= 1 && (replies[0] & NDBUS_FRAME_CODE_MASK) == NDBUS_TESTPROTO_OMD,
          "and it is addressed back to OMD 0, the OMD the request named");
    CHECK(n >= 2 && (replies[1] & NDBUS_FRAME_DATA_MASK) == 0u,
          "with our own source OMD 0 - we answer as the test protocol module");
    CHECK(nd.messages_handled == 0,
          "an OMD-0 message is NOT an ACCP command and does not touch that counter");
    CHECK(nd.testproto.messages == 1 && nd.testproto.replies == 1,
          "the test protocol counted one message and one reply");

    /* ---- get present stations: 66 words = 132 bytes, one word per station --- */
    const uint8_t get_present[] = {0x00u, 0x04u, 0x71u, 0xC7u, 0x00u, 0x0Au};
    n    = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, get_present,
                     (int)sizeof(get_present), replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 132, "get present stations is answered with 66 words = 132 bytes");
    CHECK(n == 136, "which is 136 frames - far past the 16 the reply buffer used to allow");
    CHECK(plen == 132 && reply_word(payload, 1) == 0x000Bu, "reply word1 is 0x000A plus one");
    CHECK(plen == 132 && reply_word(payload, 3) == 0u, "status Ok");
    CHECK(plen == 132 && reply_word(payload, 3 + 1) == 1u, "station 1 is reported present");
    CHECK(plen == 132 && reply_word(payload, 3 + NDBUS_STATION_ND5000_FIRST) == 1u,
          "the answering station 070B reports itself present");
    CHECK(plen == 132 && reply_word(payload, 3 + 5) == 0u,
          "station 5, which nothing registered, is reported absent");
    CHECK(plen == 132 && reply_word(payload, 3 + 10) == 0u,
          "and so is station 10 - no SCSI controller is configured here");

    /* ---- echo single word: the recorded body, pattern 1 / 0xFFFF ------------ */
    const uint8_t echo_single[] = {0x00u, 0x08u, 0x71u, 0xC7u, 0x00u, 0x0Cu,
                                   0x00u, 0x01u, 0xFFu, 0xFFu};
    n    = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, echo_single,
                     (int)sizeof(echo_single), replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 12, "echo single word is answered with 6 words = 12 bytes");
    CHECK(plen == 12 && reply_word(payload, 1) == 0x000Du, "reply word1 is 0x000C plus one");
    CHECK(plen == 12 && reply_word(payload, 4) == 0x0001u, "reply word4 echoes the pattern number");
    CHECK(plen == 12 && reply_word(payload, 5) == 0xFFFFu, "reply word5 echoes the pattern");

    /* A truncated echo request is NOT answered with a zero pattern: the missing
     * word is refused, and the station stays silent. */
    const uint8_t echo_short[] = {0x00u, 0x06u, 0x71u, 0xC7u, 0x00u, 0x0Cu, 0x00u, 0x01u};
    n = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, echo_short,
                  (int)sizeof(echo_short), replies);
    CHECK(n == 0, "an echo-single request missing its pattern word gets no reply at all");

    /* ---- echo multi word: string number, count, then the string ------------- */
    const uint8_t echo_multi[] = {0x00u, 0x0Cu, 0x71u, 0xC7u, 0x00u, 0x0Eu, 0x00u,
                                  0x07u, 0x00u, 0x02u, 0x12u, 0x34u, 0xABu, 0xCDu};
    n    = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, echo_multi,
                     (int)sizeof(echo_multi), replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 16, "echo multi word of two words is answered with 8 words = 16 bytes");
    CHECK(plen == 16 && reply_word(payload, 1) == 0x000Fu, "reply word1 is 0x000E plus one");
    CHECK(plen == 16 && reply_word(payload, 4) == 0x0007u, "reply word4 echoes the string number");
    CHECK(plen == 16 && reply_word(payload, 5) == 0x0002u, "reply word5 echoes the word count");
    CHECK(plen == 16 && reply_word(payload, 6) == 0x1234u, "reply word6 is the first string word");
    CHECK(plen == 16 && reply_word(payload, 7) == 0xABCDu, "reply word7 is the second");

    /* ---- get module type: 3 = ACCP, which is what an ND-5000 station is ----- */
    const uint8_t get_module[] = {0x00u, 0x04u, 0x71u, 0xC7u, 0x00u, 0x1Au};
    n    = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, get_module,
                     (int)sizeof(get_module), replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 10, "get module type is answered with 5 words = 10 bytes");
    CHECK(plen == 10 && reply_word(payload, 1) == 0x001Bu, "reply word1 is 0x001A plus one");
    CHECK(plen == 10 && reply_word(payload, 4) == NDBUS_TESTPROTO_MODULE_ACCP,
          "and word4 is module type 3, ACCP");

    /* ---- get test version and get Domino information ----------------------- */
    /* The VALUES in both replies are UNVERIFIED emulator placeholders, so what is
     * asserted here is the SHAPE the protocol fixes - the byte count and the
     * header - plus that the reply reports what this responder is configured
     * with, not a constant buried in the builder. */
    const uint8_t get_version[] = {0x00u, 0x04u, 0x71u, 0xC7u, 0x00u, 0x18u};
    n    = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, get_version,
                     (int)sizeof(get_version), replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 10, "get test version is answered with 5 words = 10 bytes");
    CHECK(plen == 10 && reply_word(payload, 4) == nd.testproto.test_version,
          "carrying the configured version, not a literal");

    const uint8_t get_domino[] = {0x00u, 0x04u, 0x71u, 0xC7u, 0x00u, 0x16u};
    n    = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, get_domino,
                     (int)sizeof(get_domino), replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 36, "get Domino information is answered with 18 words = 36 bytes");
    CHECK(plen == 36 && (((uint32_t)reply_word(payload, 4) << 16u) |
                         (uint32_t)reply_word(payload, 5)) == nd.testproto.processor_type,
          "words 4 and 5 are the processor type as one 32-bit number");
    CHECK(plen == 36 && memcmp(&payload[12], nd.testproto.opcom_version, 4) == 0,
          "words 6 and 7 are the four OPCOM version characters");

    /* ---- the octobus registers: the legal functions, and the reject --------- */
    const uint8_t write_reg[] = {0x00u, 0x08u, 0x71u, 0xC7u, 0x00u, 0x12u,
                                 0x00u, 0x03u, 0x5Au, 0xA5u};
    n    = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, write_reg,
                     (int)sizeof(write_reg), replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 8, "write octobus register is answered with a header-only acknowledge");
    CHECK(plen == 8 && reply_word(payload, 3) == 0u, "status Ok for function 3");

    const uint8_t write_bad[] = {0x00u, 0x08u, 0x71u, 0xC7u, 0x00u, 0x12u,
                                 0x00u, 0x04u, 0x00u, 0x01u};
    n    = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, write_bad,
                     (int)sizeof(write_bad), replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 8 && reply_word(payload, 3) == NDBUS_TESTPROTO_STATUS_BAD_REGISTER_FN,
          "an undocumented write function is refused with status 1, not with silence");

    /* Function 3 was written, so function 3 reads back - but 3 is not a legal READ
     * function, and the read of one that is legal must not see it. */
    const uint8_t read_reg[] = {0x00u, 0x06u, 0x71u, 0xC7u, 0x00u, 0x10u, 0x00u, 0x02u};
    n    = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, read_reg,
                     (int)sizeof(read_reg), replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 10, "read octobus register is answered with 5 words = 10 bytes");
    CHECK(plen == 10 && reply_word(payload, 3) == 0u, "status Ok for function 2");
    CHECK(nd.testproto.registers[3] == 0x5AA5u, "the write landed in register function 3");

    /* ---- what is NOT a test protocol message ------------------------------- */
    /* No magic: not a Test Protocol message at all, so no reply - as opposed to a
     * reply saying the command was wrong. */
    const uint8_t no_magic[] = {0x00u, 0x04u, 0x12u, 0x34u, 0x00u, 0x00u};
    n = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, no_magic,
                  (int)sizeof(no_magic), replies);
    CHECK(n == 0, "a body without the 0x71C7 magic gets no reply");

    /* An unknown command IS answered: status 2, illegal Test Protocol command
     * code. Silence there would mean "no station", and the station is present. */
    const uint8_t unknown[] = {0x00u, 0x04u, 0x71u, 0xC7u, 0x00u, 0x44u};
    n    = send_omd0(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, unknown,
                     (int)sizeof(unknown), replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 8, "an unknown command still gets the 4-word header");
    CHECK(plen == 8 && reply_word(payload, 1) == 0x0045u, "with the command plus one");
    CHECK(plen == 8 && reply_word(payload, 3) == NDBUS_TESTPROTO_STATUS_BAD_COMMAND,
          "and status 2, illegal Test Protocol command code");

    /* The OMD-0 path must not have disturbed the ACCP path: an ACCP command still
     * works, and still uses the ACCP counter. */
    uint8_t body[2];
    body[0] = (uint8_t)NDBUS_ACCP_ECHO;
    body[1] = 0;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 2, replies);
    CHECK(n == 5 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED,
          "the OMD-3 ACCP path still answers after all of that");
    CHECK(nd.messages_handled == 1, "and it is the ACCP counter that moved, not the OMD-0 one");

    /* An OMD nobody serves is still the microprogram's, not a protocol here. */
    uint16_t dest = (uint16_t)((uint16_t)NDBUS_STATION_ND5000_FIRST << NDBUS_FRAME_STATION_SHIFT);
    unsigned long tp_before = nd.testproto.messages;
    (void)ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU,
                            (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                                       NDBUS_FRAME_S_STARTSTOP | 5u),
                            replies);
    (void)ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU,
                            (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE | 5u),
                            replies);
    CHECK(nd.testproto.messages == tp_before, "an OMD-5 message is neither protocol's");

    ndbus_pool_destroy(&pool);
}

/* ---------------------------------------------------------------------------
 * Layer 13: the mailbox servicer - the ND-5000 microprogram's side.
 *
 * PORTED ORACLE. Every address, value and expectation below comes from
 * $RETROCORE/Emulated.Tests.ND100/ControllerOctobus/OctobusMailboxO1Tests.cs,
 * which is the test RetroCore pins its own station against, and whose own header
 * names the carved SINTRAN producer it replays:
 *     ITO500XQ  link the message into the X5BEX ex-queue chain
 *     ITOFIFOQ  insert into the X5FIF ring at X5FYL, advance mod X5MXF
 *     ACT51     X5ACT := 0, the idle-wakeup doorbell, and NO kick
 * The fixture's MPM_BASE 0x00420000 is dropped because nd500x addresses the pool
 * relative to its own start, so HEADER is pool offset 0 and every other offset is
 * the RetroCore constant minus MPM_BASE. Nothing else is changed.
 *
 * THE N5STA GATE ROWS ARE THE POINT OF THE LAYER. A live octobus run parks a
 * 3SWMESS at SWPPI(6) and stalls, which makes "the servicer ignores the swapper
 * message" look like the defect. It is not: SWPPI is an ND-100-side state and the
 * walk's gate is N5STA == 1, which is what the real B30 control store does too.
 * Every negative row rings the doorbell and asserts X5ACT came back re-armed to 1,
 * so "not serviced" can never be "no doorbell was rung", and the positive row runs
 * the same harness - a servicer that answers nothing fails loudly instead of
 * passing every row.
 */

/* THE WHOLE FIXTURE SITS AT A NON-ZERO POOL OFFSET, and that is not cosmetic.
 * RetroCore's layout is relative to its MPM_BASE, so its header lands at
 * window-relative 0; nd500x uses start_mess == 0 to mean "no mailbox has been
 * located yet", the state ndbus_nd5000_service_mailbox() and the GIVEINT tail both
 * refuse to act in. A header at offset 0 is therefore indistinguishable from no
 * header at all. MBX_BASE shifts the whole structure and leaves every distance
 * between its parts exactly as the oracle has them. */
#define MBX_BASE       0x4000u     /* where the oracle's window base lands here */
#define MBX_HEADER     MBX_BASE            /* X500DF global header, stride slot 0 */
#define MBX_EXT1       (MBX_BASE + 256u)   /* CPU 1 extension block, 200B words */
#define MBX_RING       (MBX_BASE + 0x800u) /* X5FIF ring storage */
#define MBX_MSG        (MBX_BASE + 0x1000u)/* the message block */
#define MBX_RING_SLOTS 8u                  /* X5MXF */

/** XMSINIT's picture: the global header plus the CPU-1 extension block. */
static void mbx_init_structures(NdbusPool *pool)
{
    (void)ndbus_pool_write16(pool, MBX_HEADER + NDBUS_MBX_X5SEM_WORD * 2u, 0);
    (void)ndbus_pool_write16(pool, MBX_HEADER + NDBUS_MBX_X5HEN_WORD * 2u, 0);
    (void)ndbus_pool_write16(pool, MBX_HEADER + NDBUS_MBX_X5FYL_WORD * 2u, 0);
    (void)ndbus_pool_write16(pool, MBX_HEADER + NDBUS_MBX_X5MXF_WORD * 2u, MBX_RING_SLOTS);
    /* X5FIF is a BYTE offset, not a word address - see ndbus_mailbox.h. */
    (void)ndbus_pool_write16(pool, MBX_HEADER + NDBUS_MBX_X5FIF_WORD * 2u,
                             (uint16_t)(MBX_RING >> 16));
    (void)ndbus_pool_write16(pool, MBX_HEADER + (NDBUS_MBX_X5FIF_WORD + 1u) * 2u,
                             (uint16_t)(MBX_RING & 0xFFFFu));

    (void)ndbus_pool_write16(pool, MBX_EXT1 + 0u, NDBUS_MBX_X5BEX_INIT);
    (void)ndbus_pool_write16(pool, MBX_EXT1 + 2u, NDBUS_MBX_X5BEX_INIT);
    (void)ndbus_pool_write16(pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, NDBUS_MBX_X5ACT_INIT);
    (void)ndbus_pool_write16(pool, MBX_EXT1 + NDBUS_MBX_X5PRO_WORD * 2u, NDBUS_MBX_X5PRO_INIT);
}

/** A message block addressed to the ND-500, LINK = -1. */
static void mbx_build_message(NdbusPool *pool, uint16_t micfu, uint16_t n5sta)
{
    (void)ndbus_pool_write16(pool, MBX_MSG + 0u, 0xFFFFu);
    (void)ndbus_pool_write16(pool, MBX_MSG + 2u, 0xFFFFu);
    (void)ndbus_pool_write16(pool, MBX_MSG + NDBUS_MSG_N5STA * 2u, n5sta);
    (void)ndbus_pool_write16(pool, MBX_MSG + NDBUS_MSG_X5CPU * 2u, 1);
    (void)ndbus_pool_write16(pool, MBX_MSG + NDBUS_MSG_MICFU * 2u, micfu);
}

/** SINTRAN's activation: ITO500XQ then ITOFIFOQ. The X5ACT write is the per-case
 *  variation and is deliberately NOT done here. */
static void mbx_replay_activation(NdbusPool *pool)
{
    (void)ndbus_pool_write16(pool, MBX_EXT1 + 0u, (uint16_t)(MBX_MSG >> 16));
    (void)ndbus_pool_write16(pool, MBX_EXT1 + 2u, (uint16_t)(MBX_MSG & 0xFFFFu));

    uint16_t fyl  = ndbus_pool_read16(pool, MBX_HEADER + NDBUS_MBX_X5FYL_WORD * 2u);
    uint32_t slot = MBX_RING + ((uint32_t)fyl * 4u);
    (void)ndbus_pool_write16(pool, slot, (uint16_t)(MBX_MSG >> 16));
    (void)ndbus_pool_write16(pool, slot + 2u, (uint16_t)(MBX_MSG & 0xFFFFu));
    (void)ndbus_pool_write16(pool, MBX_HEADER + NDBUS_MBX_X5FYL_WORD * 2u,
                             (uint16_t)((fyl + 1u) % MBX_RING_SLOTS));
}

/** Build and activate a copy-family message: addrA at msg+14, addrB at msg+18,
 *  byte count at msg+22, then ring the doorbell. */
static void mbx_copy_message(NdbusPool *pool, uint16_t micfu, uint32_t addr_a,
                             uint32_t addr_b, uint16_t count)
{
    mbx_build_message(pool, micfu, NDBUS_N5STA_TO_ND500);
    (void)ndbus_pool_write16(pool, MBX_MSG + 14u, (uint16_t)(addr_a >> 16));
    (void)ndbus_pool_write16(pool, MBX_MSG + 16u, (uint16_t)(addr_a & 0xFFFFu));
    (void)ndbus_pool_write16(pool, MBX_MSG + 18u, (uint16_t)(addr_b >> 16));
    (void)ndbus_pool_write16(pool, MBX_MSG + 20u, (uint16_t)(addr_b & 0xFFFFu));
    (void)ndbus_pool_write16(pool, MBX_MSG + 22u, count);
    mbx_replay_activation(pool);
    (void)ndbus_pool_write16(pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);
}

static uint16_t mbx_msg_status(const NdbusPool *pool)
{
    return (uint16_t)(ndbus_pool_read16(pool, MBX_MSG + NDBUS_MSG_N5STA * 2u) &
                      NDBUS_N5STA_MASK);
}

static uint16_t mbx_read(const NdbusPool *pool, uint32_t offset)
{
    return ndbus_pool_read16(pool, offset);
}

/**
 * Bring a station up with its mailbox already at MBX_HEADER and CPUNO 1, without
 * going through a control-store load: the derivation is Layer 12's subject, this
 * layer's subject is what the servicer does once a mailbox is known.
 */
static void mbx_attach(NdbusNd5000 *nd, NdbusPool *pool, NdbusFabric *fabric)
{
    CHECK(ndbus_nd5000_init(nd, NDBUS_STATION_ND5000_FIRST, pool, NULL, NULL),
          "a station at 070B for the mailbox servicer");
    CHECK(ndbus_fabric_register(fabric, &nd->station), "registered on the fabric");
    CHECK(ndbus_mailbox_attach(&nd->mailbox, pool, MBX_HEADER, 1),
          "the mailbox attached at the header, CPUNO 1");
    CHECK(ndbus_servicer_set_header(&nd->servicer, MBX_HEADER),
          "the servicer told where the header is");
    /* start_mess is what the poll and the GIVEINT tail test for a located
     * mailbox. Layer 12 covers deriving it from the control store; here it is set
     * directly, which is why MBX_BASE has to be non-zero. */
    nd->start_mess = MBX_HEADER;
}

/* A stand-in process host, so the taken and declined paths can both be exercised
 * without an ND-500 CPU in the test. */
static int      s_start_calls;
static uint32_t s_start_ctx_byte;
static bool     s_start_take;

static bool test_start_process(void *ctx, uint32_t msg_byte, uint16_t micfu, uint32_t ctx_byte)
{
    (void)ctx;
    (void)msg_byte;
    (void)micfu;
    s_start_calls++;
    s_start_ctx_byte = ctx_byte;
    return s_start_take;
}

static void test_mailbox_servicer(void)
{
    printf("Layer 13: the mailbox servicer, ported from RetroCore's O1 oracle\n");

    /* --- the N5STA gate, one case per RetroCore TestCase row ---------------- */
    struct
    {
        uint16_t    n5sta;
        uint16_t    micfu;
        bool        expect_serviced;
        const char *name;
    } gate[] = {
        { NDBUS_N5STA_TO_ND500, NDBUS_MICFU_RMICV, true,
          "MSGN500 is serviced - the positive control" },
        { 6u, NDBUS_MICFU_SWMESS, false,
          "SWPPING with 3SWMESS is walked past - the measured stall" },
        { 6u, NDBUS_MICFU_RMICV, false,
          "SWPPING wins over an otherwise serviceable MICFU" },
        { 7u, NDBUS_MICFU_SWMESS, false, "PSWWAIT is walked past too" },
    };

    for (size_t i = 0; i < sizeof gate / sizeof gate[0]; i++)
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for a gate row");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        mbx_build_message(&pool, gate[i].micfu, gate[i].n5sta);
        mbx_replay_activation(&pool);
        /* ACT51: every row rings the doorbell, so "not serviced" can never be
         * "no doorbell". */
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        bool serviced = ndbus_nd5000_service_mailbox(&nd);

        CHECK(mbx_read(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u) == 1u,
              "X5ACT re-armed to 1 before the walk, so the walk provably ran");
        CHECK(serviced == gate[i].expect_serviced, gate[i].name);
        if (gate[i].expect_serviced)
        {
            CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ANSWER,
                  "the serviced message carries ANSWER(3)");
        }
        else
        {
            CHECK(mbx_msg_status(&pool) == gate[i].n5sta,
                  "the refused message keeps its own N5STA untouched");
            CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_MICFU * 2u) == gate[i].micfu,
                  "and its MICFU untouched");
        }

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- the idle path ----------------------------------------------------- */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the idle path");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);
        mbx_build_message(&pool, NDBUS_MICFU_RMICV, NDBUS_N5STA_TO_ND500);
        mbx_replay_activation(&pool);

        /* X5ACT still -1: nothing pending, and nothing must happen. This is what
         * a missing doorbell looks like, which is what makes every negative row
         * above non-vacuous. */
        CHECK(!ndbus_nd5000_service_mailbox(&nd),
              "X5ACT idle: the poll does nothing");
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_TO_ND500,
              "and the message is left ToNd500");

        /* Now ring it. No kick anywhere in this path. */
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);
        CHECK(ndbus_nd5000_service_mailbox(&nd),
              "X5ACT zero: the message is serviced with no kick at all");
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ANSWER, "answered ANSWER(3)");

        /* 3RMICV answers TWO halfwords: version into the N500A slot, CPU
         * parameter into 10B. Microcode-verified 015332-015334. */
        CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_N500A * 2u) ==
                  NDBUS_SERVICER_MICRO_VERSION_DEFAULT,
              "3RMICV wrote the microprogram version into the N500A slot");
        CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_SWRST * 2u) ==
                  NDBUS_SERVICER_CPU_PARAMETER_DEFAULT,
              "3RMICV wrote the CPU parameter into word 10B");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- the answer ring -------------------------------------------------- */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the answer ring");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);
        mbx_build_message(&pool, NDBUS_MICFU_RMICV, NDBUS_N5STA_TO_ND500);
        mbx_replay_activation(&pool);   /* leaves X5FYL at 1 */
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "the message is answered");
        CHECK(mbx_read(&pool, MBX_HEADER + NDBUS_MBX_X5FYL_WORD * 2u) == 2u,
              "X5FYL advanced by exactly one slot");

        uint32_t slot = MBX_RING + 1u * 4u;
        uint32_t stored = ((uint32_t)mbx_read(&pool, slot) << 16) |
                          mbx_read(&pool, slot + 2u);
        CHECK(stored == MBX_MSG,
              "the answered message's own pointer is in the slot X5FYL named");
        CHECK(mbx_read(&pool, MBX_HEADER + NDBUS_MBX_X5HEN_WORD * 2u) == 0u,
              "X5HEN is the ND-100's drain index and is NOT touched");
        CHECK(mbx_read(&pool, MBX_HEADER + NDBUS_MBX_X5SEM_WORD * 2u) ==
                  NDBUS_MBX_X5SEM_FREE,
              "X5SEM is released after the answer");
        CHECK(nd.servicer.answer_ring_inserted == 1u, "one ring insert counted");
        CHECK(nd.servicer.answer_sem_taken == 1u, "X5SEM was held across the answer");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- a full ring still answers ---------------------------------------- */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the full ring");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);
        mbx_build_message(&pool, NDBUS_MICFU_RMICV, NDBUS_N5STA_TO_ND500);
        mbx_replay_activation(&pool);
        /* X5FYL 1, X5HEN 2: the next slot is the drain index, so the ring is
         * full. The microcode skips the insert and still interrupts. */
        (void)ndbus_pool_write16(&pool, MBX_HEADER + NDBUS_MBX_X5FYL_WORD * 2u, 1);
        (void)ndbus_pool_write16(&pool, MBX_HEADER + NDBUS_MBX_X5HEN_WORD * 2u, 2);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "a full ring still answers");
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ANSWER, "N5STA is still ANSWER(3)");
        CHECK(mbx_read(&pool, MBX_HEADER + NDBUS_MBX_X5FYL_WORD * 2u) == 1u,
              "X5FYL unchanged on a full ring");
        CHECK(nd.servicer.answer_ring_skipped_full == 1u,
              "and the skip is COUNTED, not silent");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- the GIVEINT answer frame, composed from the captured 5OMDNO ------- */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the GIVEINT frame");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        /* LSYSPAR word 1 = 0x0800 - S5, whose high byte is 5OMDNO 10B. This is the
         * value RetroCore's OctobusPhase3MonBringupTests.CaptureSysparOmd asserts as
         * the PRECONDITION of its MON-answer test, and it is what the capture path
         * produces from the real six-byte payload (see the frame-level check below).
         * NEVER HARDCODE THE FRAME: it is composed from this. */
        nd.lsyspar_word1 = 0x0108u;

        mbx_build_message(&pool, NDBUS_MICFU_RMICV, NDBUS_N5STA_TO_ND500);
        mbx_replay_activation(&pool);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "the message is answered");
        CHECK(nd.giveint_frames == 1u, "one GIVEINT frame was handed to the fabric");
        /* (0x0108 AND 0x3F00) OR 0x8001 = 0x8101 = 100401B, the word observed on the
         * live machine. NO SHIFT - word 1 holds the IDENT byte in bits 15-8, so the
         * 0x3F00 mask takes it directly as the destination station. Verified in the
         * current RetroCore source, OctobusND5000Station.cs:2273. */
        CHECK(nd.last_giveint_frame == 0x8101u,
              "the frame is (word1 AND 0x3F00) OR 0x8001 = 100401B");
        CHECK((nd.last_giveint_frame & 0x8000u) == 0x8000u, "C bit set");
        CHECK(((nd.last_giveint_frame >> 8) & 0x3Fu) == 1u,
              "destination station 1 - the ND-100");
        CHECK((nd.last_giveint_frame & 0x00FFu) == 0x0001u, "information byte 1");

        /* And the failure RetroCore measures on its own configuration: 5OMDNO 3
         * composes to destination station 0, which the fabric drops. Asserted so
         * the arithmetic is pinned in BOTH directions rather than only the one
         * that works. */
        /* A word 1 with NO ident byte - the shape a one- or two-byte-late parse
         * produces. Its station field is 0 and the fabric drops it, which is the
         * failure both of nd500x's earlier parses caused. */
        nd.lsyspar_word1 = 0x0008u;
        mbx_build_message(&pool, NDBUS_MICFU_RMICV, NDBUS_N5STA_TO_ND500);
        mbx_replay_activation(&pool);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);
        CHECK(ndbus_nd5000_service_mailbox(&nd), "it still answers");
        CHECK(nd.last_giveint_frame == 0x8001u,
              "a word 1 without the ident byte composes to 0x8001");
        CHECK(((nd.last_giveint_frame >> 8) & 0x3Fu) == 0u,
              "whose destination field is 0, the station the fabric drops");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- CONTMIC and RESTMIC start the microprogram, not just STARTMIC --------
     *
     * RetroCore handles 066B STARTMIC, 035B CONTMIC and 036B RESTMIC in one arm, all
     * three setting the running flag. nd500x armed only STARTMIC, so a CONTMIC left
     * the flag false - and that flag gates ENKICK's model report, STOPMIC's refusal,
     * and the whole guard matrix. */
    {
        const uint8_t starters[3] = { (uint8_t)NDBUS_ACCP_STARTMIC,
                                      (uint8_t)NDBUS_ACCP_CONTMIC,
                                      (uint8_t)NDBUS_ACCP_RESTMIC };
        const char *names[3] = { "STARTMIC 066B", "CONTMIC 035B", "RESTMIC 036B" };
        for (int k = 0; k < 3; k++)
        {
            NdbusPool pool;
            CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for a start command");
            NdbusFabric fabric;
            ndbus_fabric_init(&fabric, NULL);
            NdbusNd5000 nd;
            CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, NULL, NULL),
                  "a station for a start command");
            CHECK(ndbus_fabric_register(&fabric, &nd.station), "registered on the fabric");

            CHECK(!nd.accp.microprogram_running, "the microprogram starts not running");

            /* FOUR PARAMETER BYTES, because the minimum differs per command and a
             * short message is refused in SILENCE: STARTMIC takes 2, CONTMIC 0 and
             * RESTMIC 4 (ndbus_accp_min_parameter_bytes). Sending 2 made RESTMIC look
             * like a missing state transition when it was a short message being
             * correctly refused. */
            uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
            uint8_t  body[8];
            body[0] = starters[k];
            body[1] = 0x00u;
            body[2] = 0x00u;
            body[3] = 0x00u;
            body[4] = 0x00u;
            (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST,
                            body, 5, replies);
            CHECK(nd.accp.microprogram_running, names[k]);
            CHECK(!nd.accp_idle, "and the ACCP is not left idle");

            ndbus_nd5000_destroy(&nd);
            ndbus_pool_destroy(&pool);
        }
    }

    /* --- ENKICK sends the microprogram model/version report ------------------
     *
     * RetroCore's ENKICK answers with the bare acknowledge AND an unsolicited
     * TRAP_OCBM message carrying the model and version. Its own comment at the call
     * site says why the ack alone is not enough: the ND-100 monitor is in a busy-wait
     * that only an inbound octobus frame breaks, and the report carries the
     * control-store-derived model byte that gates "Wrong microprogram" (EWRON).
     * nd500x sent only the ack.
     *
     * Layer 12 covers LOADING the control store, so this block pokes the two source
     * halfwords directly and pins the REPORT: six bytes
     *     [0x82][0x01][model][model][ver hi][ver lo]
     * to the runtime 5OMDNO from LSYSPAR S5, with SOURCE OMD 4 rather than the 3 an
     * ordinary ACCP reply uses. Ported from OctobusND5000Station.cs:3134. */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 256 * 1024), "a pool for the model report");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, NULL, NULL),
              "a station for the model report");
        CHECK(ndbus_fabric_register(&fabric, &nd.station), "registered on the fabric");

        uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
        uint8_t  body[8];

        /* A control store has to exist before anything can be read out of it. One
         * LPARP + one LOCSM pulse is the cheapest way to get it allocated. */
        const uint32_t pb = 0x00000800u;
        body[0] = (uint8_t)NDBUS_ACCP_LPARP;
        body[1] = (uint8_t)(pb >> 24u);
        body[2] = (uint8_t)(pb >> 16u);
        body[3] = (uint8_t)(pb >> 8u);
        body[4] = (uint8_t)(pb & 0xFFu);
        (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST,
                        body, 5, replies);
        (void)ndbus_pool_write16(&pool, pb, 1u);          /* N = 1 word */
        (void)ndbus_pool_write16(&pool, pb + 2u, 0u);     /* at CS address 0 */
        body[0] = (uint8_t)NDBUS_ACCP_LOCSM;
        (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST,
                        body, 1, replies);
        CHECK(nd.control_store != NULL, "the control store is allocated");

        if (nd.control_store != NULL)
        {
            /* The two cells the report is built from: word 1 halfword 7 is the LARG
             * version, word 7 halfword 7's low byte is CPUMODEL. The values are the
             * ones RetroCore measured on the real 5800-B30 image. */
            nd.control_store[1u * NDBUS_CS_HALFWORDS_PER_WORD + 7u] = 0x2E9Au;
            nd.control_store[7u * NDBUS_CS_HALFWORDS_PER_WORD + 7u] = 0x0038u;
        }

        /* 5OMDNO 8 in S5, and a running microprogram - the report is sent only then. */
        nd.lsyspar_word1 = 0x0800u;
        nd.accp.microprogram_running = true;

        body[0] = (uint8_t)NDBUS_ACCP_ENKICK;
        int n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST,
                          body, 1, replies);
        CHECK(n > 0, "ENKICK is acknowledged");
        CHECK(nd.model_reports == 1u, "and the model/version report is sent as well");
        CHECK(nd.last_model_report_model == 0x38u,
              "the model byte is CS word 7 halfword 7 low byte - 0x38");
        CHECK(nd.last_model_report_version == 0x2E9Au,
              "the version is CS word 1 halfword 7 - 0x2E9A");

        /* A station with no control store must not invent a report. */
        NdbusNd5000 bare;
        CHECK(ndbus_nd5000_init(&bare, NDBUS_STATION_ND5000_FIRST + 1u, &pool, NULL, NULL),
              "a second station with no control store");
        CHECK(ndbus_fabric_register(&fabric, &bare.station), "also registered");
        bare.lsyspar_word1 = 0x0800u;
        bare.accp.microprogram_running = true;
        body[0] = (uint8_t)NDBUS_ACCP_ENKICK;
        (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU,
                        (uint8_t)(NDBUS_STATION_ND5000_FIRST + 1u), body, 1, replies);
        CHECK(bare.model_reports == 0u, "no control store, no report - nothing invented");
        CHECK(bare.report_no_store == 1u, "and the skip is COUNTED, not silent");

        ndbus_nd5000_destroy(&bare);
        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- LSYSPAR: word 1 comes off the WIRE, not out of a test assignment ---
     *
     * WHY THIS EXISTS. The block above sets nd.lsyspar_word1 directly, so it proved
     * the GIVEINT arithmetic and NOTHING about the capture that feeds it. The live
     * capture read two bytes too far and produced 0x0000 for a real message - every
     * answer frame then composed as 0x8001, destination station 0, dropped by the
     * fabric, and SINTRAN never received an answer interrupt. A test that assigns the
     * field cannot see that, so this one plays the real six-byte LSYSPAR message.
     *
     * The frames and their meaning are RetroCore's, from
     * OctobusPhase3MonBringupTests.CaptureSysparOmd:
     *     SOMB(code 3), 0x01, 0x08, 0x0E=SystemParameter,
     *     0x01 N100IDENT, 0x08 S5 hi = 5OMDNO 10B, 0x00 S5 lo, 0x00, 0x00, 0x00,
     *     EOMB(code 3)
     * The payload is ONE BYTE then the words, so S5 starts at the second parameter
     * byte. Reading it as the second 16-bit WORD is what was wrong. */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the LSYSPAR capture");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        CHECK(nd.lsyspar_word1 == 0u, "nothing captured before the message arrives");

        /* frame = flags | (station << 8) | info; C=0x8000, M=0x20, S=0x10. */
        static const uint8_t body[] = { 0x01u, 0x08u, 0x0Eu,
                                        0x01u, 0x08u, 0x00u, 0x00u, 0x00u, 0x00u };
        uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
        uint16_t nd5000_last_replies[NDBUS_MAX_REPLY_FRAMES];
        int      nd5000_last_reply_count = 0;
        (void)ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU,
                                (uint16_t)(0x8000u | 0x0020u | 0x0010u
                                           | (NDBUS_STATION_ND5000_FIRST << 8) | 0x03u),
                                replies);
        for (size_t i = 0; i < sizeof body; i++)
        {
            (void)ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU,
                                    (uint16_t)((NDBUS_STATION_ND5000_FIRST << 8) | body[i]),
                                    replies);
        }
        /* The EOMB is the frame that completes the message, so it is the one whose
         * reply carries the presence ack. Keep it. */
        nd5000_last_reply_count =
            ndbus_fabric_send(&fabric, NDBUS_STATION_ND120_CPU,
                              (uint16_t)(0x8000u | 0x0020u
                                         | (NDBUS_STATION_ND5000_FIRST << 8) | 0x03u),
                              nd5000_last_replies);

        CHECK(nd.lsyspar_word1 == 0x0108u,
              "word 1 captured from the wire is 0x0108 - ident byte then 5OMDNO 10B");
        CHECK(nd.lsyspar_word1 != 0x0800u,
              "and NOT 0x0800, the one-byte-late parse RetroCore warns against");
        CHECK(nd.lsyspar_word1 != 0x0000u,
              "and NOT zero, the two-byte-late parse nd500x started with");

        /* THE CMSYSPAR IS ANSWERED, AND ON THE SENDING OMD.
         *
         * SINTRAN's CON5IDENT sends CMSYSPAR and waits (GO I5OMBR) for a multibyte
         * MFACK; consuming the command is not enough. WHICH OMD the ack goes to was
         * settled by measurement, not by reading: RetroCore answers on the S5 OMD
         * (SendAccpMessack(message[4])), nd500x was changed to match, and the live
         * monitor then printed at entry
         *     ND-5000 timeout: ACCP was terminated; Microprogram is running
         * which it does not print when the ack goes to the OMD the command arrived on.
         * So this pins the SOURCE OMD, and the disagreement with the oracle is recorded
         * in ndbus_nd5000.c rather than hidden.
         *
         * The destination OMD is the low nibble of the SOMB frame
         * (ndbus_multibyte_build), and MFACK is payload byte 0 = 0x00. */
        int ack_frames = nd5000_last_reply_count;
        CHECK(ack_frames > 0, "the CMSYSPAR is ANSWERED, not merely consumed");
        if (ack_frames > 0)
        {
            uint16_t somb = nd5000_last_replies[0];
            /* The source OMD is body[0], which this message sets to 0x01 - NOT the
             * SOMB frame's own code field, which is 3 (the ACCP command library). The
             * two are different fields and the reply keys off body[0]. */
            CHECK((somb & 0x000Fu) == 0x01u,
                  "the ack goes to the OMD in body[0], the sending OMD");
            CHECK((somb & 0x000Fu) != 0x08u,
                  "and not to the S5 OMD, which regressed the live monitor");
            CHECK((somb & NDBUS_FRAME_C_CONTROL) != 0u, "SOMB carries the C bit");
            CHECK((somb & NDBUS_FRAME_M_MULTIBYTE) != 0u, "and the M bit");
        }

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- no mailbox located: the poll refuses ----------------------------- */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the unlocated case");
        NdbusNd5000 nd;
        CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, NULL, NULL),
              "a station with no mailbox derived yet");
        mbx_init_structures(&pool);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);
        CHECK(!ndbus_nd5000_service_mailbox(&nd),
              "with no mailbox located the poll does nothing at all");
        CHECK(nd.service_polls == 0u,
              "and does not even count as a poll - there is nothing to poll");
        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- a two-node chain, which no live trace has yet produced ------------ */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the chain walk");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        /* Head at MBX_MSG links to a second block; the second ends the chain.
         * RetroCore's own note: every live trace so far chains exactly ONE
         * message, so multi-node chains are test-covered only. */
        const uint32_t second = MBX_MSG + 0x200u;
        mbx_build_message(&pool, NDBUS_MICFU_RMICV, NDBUS_N5STA_TO_ND500);
        (void)ndbus_pool_write16(&pool, MBX_MSG + 0u, (uint16_t)(second >> 16));
        (void)ndbus_pool_write16(&pool, MBX_MSG + 2u, (uint16_t)(second & 0xFFFFu));
        (void)ndbus_pool_write16(&pool, second + 0u, 0xFFFFu);
        (void)ndbus_pool_write16(&pool, second + 2u, 0xFFFFu);
        (void)ndbus_pool_write16(&pool, second + NDBUS_MSG_N5STA * 2u,
                                 NDBUS_N5STA_TO_ND500);
        (void)ndbus_pool_write16(&pool, second + NDBUS_MSG_MICFU * 2u,
                                 NDBUS_MICFU_RMICV);
        mbx_replay_activation(&pool);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "the chain is serviced");
        CHECK(nd.servicer.nodes_walked == 2u, "both nodes were walked");
        CHECK(nd.servicer.messages_answered == 2u, "both were answered");
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ANSWER, "the head answered");
        CHECK((ndbus_pool_read16(&pool, second + NDBUS_MSG_N5STA * 2u) &
               NDBUS_N5STA_MASK) == NDBUS_N5STA_ANSWER,
              "and so did the second node the LINK pointed at");
        CHECK(nd.servicer.answer_ring_inserted == 2u, "two ring inserts");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- an unported MICFU answers 5ERANSWER, and is counted -------------- */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the 5ERANSWER case");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        /* 21B 3WREG. 5ERANSWER(4) here is not a gap in this port - it is what the
         * ND-5000 itself answers, because 3WREG is MSG_ILLEG in both 5800 listings
         * (B30 @015245, A30 @014261). The histogram must still say 21B arrived,
         * which is how the next code to port gets chosen from a measurement rather
         * than a guess.
         *
         * NOT 23B 3START: that is a start-class code and answers ANSWER(3) on the
         * declined path, covered separately below. */
        mbx_build_message(&pool, NDBUS_MICFU_WREG, NDBUS_N5STA_TO_ND500);
        mbx_replay_activation(&pool);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "an unported MICFU is still answered");
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ERROR_ANSWER,
              "with 5ERANSWER(4), not ANSWER(3)");
        CHECK(nd.servicer.messages_declined == 1u, "the decline is counted");
        CHECK(nd.servicer.micfu_counts[NDBUS_MICFU_WREG] == 1u,
              "and the histogram records WHICH code arrived");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- the power-fail bits survive an answer ---------------------------- */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the power-fail bits");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        /* 160000B in N5STA's top bits belongs to the ND-100 driver. The answer
         * must not clear it. */
        mbx_build_message(&pool, NDBUS_MICFU_RMICV,
                          (uint16_t)(NDBUS_N5STA_PF_MASK | NDBUS_N5STA_TO_ND500));
        mbx_replay_activation(&pool);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        CHECK(ndbus_nd5000_service_mailbox(&nd),
              "a message carrying power-fail flags is still ours");
        uint16_t sta = ndbus_pool_read16(&pool, MBX_MSG + NDBUS_MSG_N5STA * 2u);
        CHECK((sta & NDBUS_N5STA_MASK) == NDBUS_N5STA_ANSWER, "answered ANSWER(3)");
        CHECK((sta & NDBUS_N5STA_PF_MASK) == NDBUS_N5STA_PF_MASK,
              "and the power-fail bits were preserved, not cleared");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- 12B CACHE and 22B STARTP0 both answer ANSWER(3) ------------------- */
    {
        struct
        {
            uint16_t    micfu;
            const char *name;
        } accepted[] = {
            { NDBUS_MICFU_CACHE,
              "12B CACHE answers ANSWER(3) - a 5ERANSWER here aborts the swapper load" },
            { NDBUS_MICFU_STARTP0, "22B STARTP0 answers ANSWER(3) like the microcode's MSG_END" },
        };

        for (size_t i = 0; i < sizeof accepted / sizeof accepted[0]; i++)
        {
            NdbusPool pool;
            CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for an accepted MICFU");
            NdbusFabric fabric;
            ndbus_fabric_init(&fabric, NULL);
            NdbusNd5000 nd;
            mbx_init_structures(&pool);
            mbx_attach(&nd, &pool, &fabric);

            mbx_build_message(&pool, accepted[i].micfu, NDBUS_N5STA_TO_ND500);
            mbx_replay_activation(&pool);
            (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

            CHECK(ndbus_nd5000_service_mailbox(&nd), "it is serviced");
            CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ANSWER, accepted[i].name);
            CHECK(nd.servicer.messages_declined == 0u, "and nothing was declined");

            ndbus_nd5000_destroy(&nd);
            ndbus_pool_destroy(&pool);
        }
    }

    /* --- the copy family, byte-exact -------------------------------------- */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the copy family");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        const uint32_t target = MBX_BASE + 0x2000u;  /* addrA, the ND-500 side */
        const uint32_t buffer = MBX_BASE + 0x2400u;  /* addrB, the buffer side */

        /* THE EXACT DEFECT ROUNDING CAUSED. A 2-byte write must leave the NEXT
         * halfword alone: RetroCore rounded the count up to 4 here and overwrote a
         * live capability in that halfword, which turned a domain's data segment
         * read-only and produced four unrelated-looking symptoms. */
        (void)ndbus_pool_write16(&pool, buffer + 0u, 0xC00Cu);
        (void)ndbus_pool_write16(&pool, buffer + 2u, 0x1111u);
        (void)ndbus_pool_write16(&pool, target + 0u, 0x0000u);
        (void)ndbus_pool_write16(&pool, target + 2u, 0x000Bu);

        mbx_copy_message(&pool, NDBUS_MICFU_RESIWR, target, buffer, 2u);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "14B RESIWR is serviced");
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ANSWER, "and answered ANSWER(3)");
        CHECK(mbx_read(&pool, target + 0u) == 0xC00Cu, "the two bytes asked for were written");
        CHECK(mbx_read(&pool, target + 2u) == 0x000Bu,
              "and the NEXT halfword is untouched - the count is never rounded up");
        CHECK(nd.servicer.copies_done == 1u && nd.servicer.copy_bytes == 2u,
              "one copy of exactly two bytes counted");

        /* An odd count of 1 must leave the other byte of the halfword alone. */
        (void)ndbus_pool_write16(&pool, buffer + 0u, 0xAA55u);
        (void)ndbus_pool_write16(&pool, target + 0u, 0x1234u);
        mbx_copy_message(&pool, NDBUS_MICFU_RESIWR, target, buffer, 1u);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "a one-byte copy is serviced");
        CHECK(mbx_read(&pool, target + 0u) == 0xAA34u,
              "the high byte came from the source and the low byte survived");

        /* A READ moves the other way: target A -> buffer B. */
        (void)ndbus_pool_write16(&pool, target + 0u, 0x7788u);
        (void)ndbus_pool_write16(&pool, buffer + 0u, 0x0000u);
        mbx_copy_message(&pool, NDBUS_MICFU_RESIRD, target, buffer, 2u);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "13B RESIRD is serviced");
        CHECK(mbx_read(&pool, buffer + 0u) == 0x7788u, "a READ moves A into B");

        /* A transfer that leaves the pool is refused, not silently zero-filled. */
        mbx_build_message(&pool, NDBUS_MICFU_RESIWR, NDBUS_N5STA_TO_ND500);
        (void)ndbus_pool_write16(&pool, MBX_MSG + 14u, 0xFFFFu);
        (void)ndbus_pool_write16(&pool, MBX_MSG + 16u, 0xF000u);
        (void)ndbus_pool_write16(&pool, MBX_MSG + 18u, (uint16_t)(buffer >> 16));
        (void)ndbus_pool_write16(&pool, MBX_MSG + 20u, (uint16_t)(buffer & 0xFFFFu));
        (void)ndbus_pool_write16(&pool, MBX_MSG + 22u, 16u);
        mbx_replay_activation(&pool);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "an out-of-pool copy still answers");
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ERROR_ANSWER,
              "with 5ERANSWER(4), because the transfer did not happen");
        CHECK(nd.servicer.copies_refused == 1u, "and the refusal is counted");

        /* PHYSWR with no PST base known: the fallback happens and is COUNTED, so a
         * transfer that may have landed in the wrong cell is never silent. */
        CHECK(nd.servicer.pst_base == 0u, "no physical segment table base is known");
        mbx_build_message(&pool, NDBUS_MICFU_PHYSWR, NDBUS_N5STA_TO_ND500);
        (void)ndbus_pool_write16(&pool, MBX_MSG + 14u, (uint16_t)(target >> 16));
        (void)ndbus_pool_write16(&pool, MBX_MSG + 16u, (uint16_t)(target & 0xFFFFu));
        (void)ndbus_pool_write16(&pool, MBX_MSG + 18u, (uint16_t)(buffer >> 16));
        (void)ndbus_pool_write16(&pool, MBX_MSG + 20u, (uint16_t)(buffer & 0xFFFFu));
        (void)ndbus_pool_write16(&pool, MBX_MSG + 22u, 2u);
        (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_MSWMC * 2u, 10u);
        mbx_replay_activation(&pool);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "31B PHYSWR is serviced");
        CHECK(nd.servicer.segment_unresolved == 1u,
              "the unresolved segment is counted, never silently treated as flat");
        CHECK(nd.servicer.segment_resolved == 0u, "and nothing was resolved");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- the start class: taken means NOT answered -------------------------- */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the start class");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        /* With no context area known the start is DECLINED and answered the no-CPU
         * way - counted, so a run where no process ever started cannot read as
         * healthy. */
        CHECK(nd.servicer.context_area_base == 0u, "no context block area is known yet");
        mbx_build_message(&pool, NDBUS_MICFU_START, NDBUS_N5STA_TO_ND500);
        mbx_replay_activation(&pool);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "23B 3START is answered when declined");
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ANSWER,
              "with ANSWER(3) - the answer a station with no CPU behind it gives");
        CHECK(nd.servicer.starts_seen == 1u && nd.servicer.starts_declined == 1u,
              "the start was seen and the decline counted");
        CHECK(nd.servicer.starts_taken == 0u, "and nothing was taken");

        /* Now with a context area AND a host that takes it: the message must be
         * left WAITING and NOT answered, because the process's stop answers it. */
        CHECK(ndbus_servicer_set_context_area(&nd.servicer, MBX_BASE + 0x3000u),
              "the context block area is set");
        s_start_calls = 0;
        s_start_ctx_byte = 0;
        s_start_take = true;
        nd.servicer.host.start_process = test_start_process;
        nd.servicer.host.ctx = NULL;

        mbx_build_message(&pool, NDBUS_MICFU_START, NDBUS_N5STA_TO_ND500);
        (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_X5CPU * 2u, 1);
        mbx_replay_activation(&pool);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        CHECK(!ndbus_nd5000_service_mailbox(&nd),
              "a taken start reports NOTHING ANSWERED");
        CHECK(s_start_calls == 1, "the process host was asked exactly once");
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_WAITING,
              "and the message is left WAITING for the process's own stop");
        CHECK(nd.servicer.starts_taken == 1u, "the take is counted");

        /* The context block address is area + 400B + X5CPU * 400B, and X5CPU is
         * ZERO-BASED - passing the mailbox's one-based CPUNO here would hand the
         * process its neighbour's registers without faulting. */
        CHECK(s_start_ctx_byte == MBX_BASE + 0x3000u + 256u + 256u,
              "the context block is area + 400B + X5CPU * 400B, X5CPU zero-based");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- the DIT base is learned from the trap-config writes --------------- */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the DIT watch");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        /* A PHYSWR landing inside the trap-config window of a process control
         * block names the table's base. The block is deliberately NOT at pool
         * offset 0, so the alignment-down is actually exercised. */
        const uint32_t pcb    = MBX_BASE + 0x5000u;
        const uint32_t buffer = MBX_BASE + 0x2400u;
        const uint32_t target = pcb + 0x96u; /* the first trap-config offset */

        CHECK(nd.servicer.dit_writes_seen == 0u, "nothing learned yet");
        (void)ndbus_pool_write16(&pool, buffer, 0x1234u);

        mbx_copy_message(&pool, NDBUS_MICFU_PHYSWR, target, buffer, 2u);
        CHECK(ndbus_nd5000_service_mailbox(&nd), "the trap-config write is serviced");
        CHECK(nd.servicer.dit_writes_seen == 1u, "and it is counted as a DIT write");
        CHECK(nd.servicer.dit_base == pcb,
              "the base is the CONTAINING 256-byte block, aligned down");

        /* A write OUTSIDE the window must not move the base, or every ordinary
         * transfer would redefine the table. */
        mbx_copy_message(&pool, NDBUS_MICFU_PHYSWR, pcb + 0x10u, buffer, 2u);
        CHECK(ndbus_nd5000_service_mailbox(&nd), "a write outside the window is serviced");
        CHECK(nd.servicer.dit_writes_seen == 1u,
              "but is NOT counted - only the trap-config window names the table");

        /* A READ never names it either: the guest is publishing nothing. */
        mbx_copy_message(&pool, NDBUS_MICFU_PHYSRD, target, buffer, 2u);
        CHECK(ndbus_nd5000_service_mailbox(&nd), "a read in the window is serviced");
        CHECK(nd.servicer.dit_writes_seen == 1u, "and a READ never names the table");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

}

/* ---------------------------------------------------------------------------
 * Layer 14: the ACCP guard matrix, every command against every state.
 *
 * PORTED ORACLE, row for row, from
 * $RETROCORE/Emulated.Tests.ND100/ControllerOctobus/AccpCommandGuardsTests.cs
 * (VerdictMatchesTheRealFirmware). Twenty-five commands times five states is 125
 * verdicts, and the point of having them all is that a guard table is exactly the
 * kind of code where one wrong cell stays invisible for months: the command that
 * cell governs is simply never sent by the boot path being tested.
 *
 * The five states are the firmware's own RAM cells in the order SINTRAN fills
 * them, so a row reads as the life of a command across bring-up:
 *   S0  fresh card
 *   S1  + LSYSPAR given          (cell 0x1143A6)
 *   S2  + LPARP given            (cell 0x1143B2)
 *   S3  + microprogram running   (cell 0x1143AC)
 *   S4  + kicks enabled          (cell 0x1143B6)
 */
/* ---------------------------------------------------------------------------
 * Layer 15: the copy family's refusals and the chain head node.
 *
 * PORTED from $RETROCORE/Nuget/HackerCorpLabs.Emulation.CPU.ND5000/tests/
 * MailboxCopyTests.cs - the Servicer half of its cases; the ones it also runs on
 * the microword engine need a CPU that executes the control store, which this
 * repository does not have.
 */
static void test_copy_family_refusals(void)
{
    printf("Layer 15: the copy family's refusals and the X5BEX head node\n");

    /* --- DMEMRD and DMEMWR must REFUSE, never copy physically -------------- */
    const uint16_t logical[2] = { NDBUS_MICFU_DMEMRD, NDBUS_MICFU_DMEMWR };
    const char    *logical_name[2] = { "10B DMEMRD", "11B DMEMWR" };

    for (size_t i = 0; i < 2; i++)
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for a logical transfer");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        const uint32_t target = MBX_BASE + 0x2000u;
        const uint32_t buffer = MBX_BASE + 0x2400u;
        (void)ndbus_pool_write16(&pool, buffer, 0xB000u);
        (void)ndbus_pool_write16(&pool, target, 0x0000u);

        mbx_copy_message(&pool, logical[i], target, buffer, 2u);
        CHECK(ndbus_nd5000_service_mailbox(&nd), "it is serviced");

        /* 5ERANSWER(4) and NOT ANSWER(3). An ANSWER here would mean the physical
         * fallback happened, which is the reference's defect B12: a logical address
         * read as a physical one returned zeros and SINTRAN reported "SEGMENT NOT
         * MODIFIABLE", and on the write half NC's prompt never appeared on any run. */
        char msg[160];
        (void)snprintf(msg, sizeof msg,
                       "%s answers 5ERANSWER(4) - an ANSWER(3) would mean it fell back to a raw "
                       "physical copy",
                       logical_name[i]);
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ERROR_ANSWER, msg);
        CHECK(nd.servicer.logical_copies_refused == 1u, "and the refusal is counted");
        CHECK(mbx_read(&pool, target) == 0x0000u,
              "the target is untouched - nothing was copied anywhere");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- THE BYTE COUNT IS EXACT. IT IS NOT ROUNDED UP TO WHOLE WORDS. -----
     *
     * RetroCore's Nd500ServicerS1Tests.ResidentWrite_14B_RoundsCountUpToFull32BitWords
     * asserts the opposite: with NRBYT = 6 it expects EIGHT bytes moved, on the
     * stated grounds that "the microcode rounds (nrbyt+3)>>2". The microcode does
     * not. Read at MSG_RESIWR, 015534-015560 of MICRO-5800-B30.LIST (the B30
     * control store, $ND5000_DECODE):
     *
     *   015537  reads the count halfword into Q         TYP,HW ... D,SC4
     *   015541  Q := Q logically shifted                ALU,XOR Q,Q/LOG
     *   015542  Q := Q logically shifted again          ALU,XOR Q,Q/LOG
     *   015543  LC := Q                                 A,Q ... D,LC
     *   015544  MSG_RESIWRW: LCDECR, loop to 015555 while LC nonzero
     *   015555  MSG_RESIWR2: a 32-bit read then a 32-bit write, back to 015544
     *   015545  LC := SARG AND count                    ALU,AND A,SARG B,SC4 D,LC
     *   015546  if that is zero, jump MSG_END
     *   015550  MSG_RESIWRBY: a BYTE read then a BYTE write, LCDECR, loop
     *
     * Two logical shifts turn the byte count into a count of 32-bit words, and
     * what is left over is copied ONE BYTE AT A TIME by a second loop. A byte
     * remainder loop cannot exist in an engine that rounds the count up - there
     * would be nothing for it to do. So NRBYT = 6 moves 6 bytes: one word plus
     * two bytes. The 7th and 8th bytes are not the microprogram's to touch.
     *
     * The manuals and the microcode are the truth and the other emulator is not,
     * so this asserts the microcode and leaves RetroCore's case failing on
     * purpose. Its defect is that a WR of 6 bytes overwrites two bytes past the
     * caller's buffer.
     */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the exact byte count");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        const uint32_t dst = MBX_BASE + 0x2000u;
        const uint32_t src = MBX_BASE + 0x2400u;

        (void)ndbus_pool_write16(&pool, src + 0u, 0x1111u);
        (void)ndbus_pool_write16(&pool, src + 2u, 0x2222u);
        (void)ndbus_pool_write16(&pool, src + 4u, 0x3333u);
        (void)ndbus_pool_write16(&pool, src + 6u, 0x4444u);

        /* A sentinel in the halfword the rounding would clobber. */
        (void)ndbus_pool_write16(&pool, dst + 6u, 0xA5A5u);

        /* 14B RESIWR moves B -> A, so A is the destination and B the source. */
        mbx_copy_message(&pool, NDBUS_MICFU_RESIWR, dst, src, 6u);
        CHECK(ndbus_nd5000_service_mailbox(&nd), "14B RESIWR with NRBYT 6 is serviced");
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ANSWER, "and answered");

        CHECK(ndbus_pool_read16(&pool, dst + 0u) == 0x1111u, "byte 0 and 1 moved");
        CHECK(ndbus_pool_read16(&pool, dst + 2u) == 0x2222u, "byte 2 and 3 moved");
        CHECK(ndbus_pool_read16(&pool, dst + 4u) == 0x3333u,
              "byte 4 and 5 moved - the two-byte remainder of 015550");
        CHECK(ndbus_pool_read16(&pool, dst + 6u) == 0xA5A5u,
              "byte 6 and 7 are UNTOUCHED: NRBYT is 6, and the microcode's byte loop "
              "runs count AND 3 times, not up to the next word boundary");
        CHECK(nd.servicer.copy_bytes == 6u, "and the station moved exactly six bytes");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- IMEMWR round-trips byte exact through RESIRD ---------------------- */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the IMEMWR round trip");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        const uint32_t istore = MBX_BASE + 0x6000u;
        const uint32_t buffer = MBX_BASE + 0x2400u;
        const uint32_t back   = MBX_BASE + 0x2800u;

        for (uint32_t w = 0; w < 4u; w++)
        {
            (void)ndbus_pool_write16(&pool, buffer + w * 2u, (uint16_t)(0xA000u + w));
        }

        mbx_copy_message(&pool, NDBUS_MICFU_IMEMWR, istore, buffer, 8u);
        CHECK(ndbus_nd5000_service_mailbox(&nd), "35B IMEMWR is serviced");

        mbx_copy_message(&pool, NDBUS_MICFU_RESIRD, istore, back, 8u);
        CHECK(ndbus_nd5000_service_mailbox(&nd), "and the block reads back");

        bool same = true;
        for (uint32_t w = 0; w < 4u; w++)
        {
            if (mbx_read(&pool, back + w * 2u) != (uint16_t)(0xA000u + w))
            {
                same = false;
            }
        }
        /* The D-space / I-space / physical distinction is a space select on real
         * hardware and ALIASES in a flat model, which is why a write through one
         * member reads back through another. */
        CHECK(same, "every byte round-tripped - the spaces alias in a flat window");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }

    /* --- a leading N5STA=0 node is walked past, and the real message answered */
    {
        NdbusPool pool;
        CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the head-node chain");
        NdbusFabric fabric;
        ndbus_fabric_init(&fabric, NULL);
        NdbusNd5000 nd;
        mbx_init_structures(&pool);
        mbx_attach(&nd, &pool, &fabric);

        /* THIS IS THE SHAPE THE LIVE MACHINE PRODUCES. SINTRAN writes X5BEX
         * pointing at a QUEUE HEAD NODE whose N5STA is 0 - measured 0xBE30 on a real
         * boot - and that node's LINK points at the actual message. Reading the head
         * node as the message makes the walk look broken; refusing N5STA != 1 and
         * following the link is what makes it work. */
        const uint32_t head = MBX_MSG;
        const uint32_t real = MBX_MSG + 0x300u;

        mbx_build_message(&pool, 0u, NDBUS_N5STA_FREE);      /* the head node */
        (void)ndbus_pool_write16(&pool, head + 0u, (uint16_t)(real >> 16));
        (void)ndbus_pool_write16(&pool, head + 2u, (uint16_t)(real & 0xFFFFu));

        (void)ndbus_pool_write16(&pool, real + 0u, 0xFFFFu); /* the real message */
        (void)ndbus_pool_write16(&pool, real + 2u, 0xFFFFu);
        (void)ndbus_pool_write16(&pool, real + NDBUS_MSG_N5STA * 2u, NDBUS_N5STA_TO_ND500);
        (void)ndbus_pool_write16(&pool, real + NDBUS_MSG_MICFU * 2u, NDBUS_MICFU_RMICV);

        mbx_replay_activation(&pool);
        (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

        CHECK(ndbus_nd5000_service_mailbox(&nd), "the chain is serviced");
        CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_FREE,
              "the head node is left alone - its N5STA is not 1, so it is not ours");
        CHECK((ndbus_pool_read16(&pool, real + NDBUS_MSG_N5STA * 2u) & NDBUS_N5STA_MASK) ==
                  NDBUS_N5STA_ANSWER,
              "and the message its LINK pointed at was answered");
        CHECK(nd.servicer.nodes_not_ours == 1u, "the skipped node is counted");

        ndbus_nd5000_destroy(&nd);
        ndbus_pool_destroy(&pool);
    }
}


static void test_accp_guard_matrix(void)
{
    printf("Layer 14: the ACCP guard matrix, 25 commands across 5 states\n");

    const NdbusAccpState state[5] = {
        /* running, syspar, pointer, kicks */
        { false, false, false, false }, /* S0 */
        { false, true, false, false },  /* S1 */
        { false, true, true, false },   /* S2 */
        { true, true, true, false },    /* S3 */
        { true, true, true, true },     /* S4 */
    };
    const char *state_name[5] = { "S0 fresh", "S1 +LSYSPAR", "S2 +LPARP", "S3 running",
                                  "S4 +kicks" };

    const int ok = NDBUS_ACCP_ACCEPTED;

    struct
    {
        uint8_t     command;
        int         verdict[5];
        const char *note;
    } row[] = {
        { 0x0Du, { 13, ok, ok, ok, ok }, "RSSYSPAR: nak 13 until LSYSPAR" },
        { 0x12u, { 1, 1, ok, -1, -1 }, "VPARP: nak 1 with no pointer, -1 once running" },
        { 0x13u, { 1, 1, ok, -1, -1 }, "LOCSM" },
        { 0x15u, { 1, 1, ok, -1, -1 }, "DUCS" },
        { 0x14u, { ok, ok, ok, -1, -1 }, "LOCSD: -1 running, measured at full length" },
        { 0x16u, { ok, ok, ok, -1, -1 }, "DCSD: -1 running, measured at full length" },
        { 0x3Cu, { 1, 1, ok, -1, -1 }, "DUCC" },
        { 0x34u, { 1, 1, ok, ok, -2 }, "LAOB32M: nak -2 once kicks are enabled" },
        { 0x35u, { 1, 1, ok, ok, -2 }, "RAIB32M" },
        { 0x25u, { ok, ok, ok, ok, -2 }, "RAIB32D" },
        { 0x22u, { ok, ok, ok, -1, -1 }, "RMIR" },
        { 0x24u, { ok, ok, ok, -1, -1 }, "RAIB16" },
        { 0x3Bu, { ok, ok, ok, -1, -1 }, "DCCD" },
        { 0x1Bu, { ok, ok, ok, -1, -1 }, "RUNTST" },
        { 0x1Cu, { 0, 0, 0, ok, ok }, "STOPMIC: nak 0 when NOT running" },
        { 0x19u, { 6, 6, 6, 6, 6 }, "a hole in the compare chain: nak 6" },
        { 0x2Eu, { 6, 6, 6, 6, 6 }, "a hole in the compare chain: nak 6" },
        { 0x18u, { ok, ok, ok, ok, ok }, "AMICTRAP: never refused" },
        { 0x28u, { ok, ok, ok, ok, ok }, "RASTS: never refused" },
        { 0x30u, { ok, ok, ok, ok, ok }, "RTEST: never refused" },
        { 0x0Eu, { ok, ok, ok, ok, ok }, "LSYSPAR: never refused" },
        { 0x11u, { ok, ok, ok, ok, ok }, "LPARP: never refused" },
        { 0x31u, { ok, ok, ok, ok, ok }, "ENKICK: never refused" },
        { 0x32u, { ok, ok, ok, ok, ok }, "DISKICK: never refused" },
        { 0x39u, { ok, ok, ok, ok, ok }, "CPURES: never refused" },
    };

    for (size_t r = 0; r < sizeof row / sizeof row[0]; r++)
    {
        for (int st = 0; st < 5; st++)
        {
            int got = ndbus_accp_evaluate(row[r].command, &state[st]);
            char msg[192];
            (void)snprintf(msg, sizeof msg, "%02X %s in %s: expected %d, got %d",
                           (unsigned)row[r].command, row[r].note, state_name[st],
                           row[r].verdict[st], got);
            CHECK(got == row[r].verdict[st], msg);
        }
    }

    /* The four holes in the compare chain, which the firmware naks 6 for, and
     * which are the reason has_arm() exists as its own question. */
    const uint8_t hole[4] = { 0x19u, 0x1Au, 0x2Eu, 0x2Fu };
    for (size_t i = 0; i < sizeof hole / sizeof hole[0]; i++)
    {
        char msg[96];
        (void)snprintf(msg, sizeof msg, "%02X has no arm in the compare chain",
                       (unsigned)hole[i]);
        CHECK(!ndbus_accp_has_arm(hole[i]), msg);
    }
    CHECK(ndbus_accp_has_arm(0x0Du), "0x0D is the first command with an arm");
    CHECK(ndbus_accp_has_arm(0x3Eu), "0x3E is the last");
    CHECK(!ndbus_accp_has_arm(0x0Cu), "below 0x0D there is no arm");
    CHECK(!ndbus_accp_has_arm(0x3Fu), "above 0x3E there is no arm");
}

/* -------------------------------------------------------------------------- */
/* The five commands that RETURN DATA, byte for byte.                         */
/*                                                                            */
/* The guard matrix above only says these are ACCEPTED. What they send back is */
/* a separate thing and was a bare canned ack until now, which is how the      */
/* ND-500 monitor came to print a module/ECO table it had never been sent -    */
/* it was rendering its own uninitialised buffer.                             */
/*                                                                            */
/* The bytes are the ones measured on the real ND-324716 firmware (state       */
/* matrix 2026-09-18) and carried in RetroCore OctobusND5000Station.cs:3289-   */
/* 3347. Four of the five are a leading status byte plus fixed data; RSSYSPAR  */
/* is the only one whose payload depends on state, and it reads back exactly   */
/* the three words LSYSPAR stored.                                            */
/*                                                                            */
/* Measured live on SINTRAN's own ND-500/5000 MONITOR J04: of these five the   */
/* monitor issues READSELFT, RECO and PRGMVERS - in that order, READSELFT as   */
/* the second command of the session and the other two after STARTMIC/ENKICK.  */
/* It issues neither RASTS nor RSSYSPAR on that path, so those two are pinned  */
/* here against the firmware measurement alone and are marked as such.        */
/* -------------------------------------------------------------------------- */
static void test_accp_data_replies(void)
{
    printf("The five data-returning ACCP commands\n");

    NdbusPool pool;
    (void)ndbus_pool_create(&pool, POOL_BYTES);

    NdbusHostOps host;
    memset(&host, 0, sizeof(host));
    host.log = counting_log;

    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);

    NdbusNd5000 nd;
    CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, &host, NULL),
          "data replies: the station comes up");
    CHECK(ndbus_fabric_register(&fabric, &nd.station), "data replies: and registers");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
    uint8_t  payload[NDBUS_MAX_REPLY_FRAMES];
    uint8_t  body[16];

    /* READSELFT (060B) is the SECOND command of a real session, before anything
     * has told the station its system parameters, so it must answer cold. Three
     * bytes: the ack and a 16-bit status word. The value is 0x0000 - what the
     * real firmware holds once CPURES has cleared it. */
    body[0] = (uint8_t)NDBUS_ACCP_READSELFT;
    int n    = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                         replies);
    int plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                                  (int)sizeof(payload));
    CHECK(plen == 3, "READSELFT answers three bytes");
    CHECK(plen == 3 && payload[0] == 0x00u && payload[1] == 0x00u && payload[2] == 0x00u,
          "READSELFT: ack plus a zero 16-bit self-test status");

    /* RSSYSPAR (0x0D) naks 13 before LSYSPAR - the guard matrix pins that - so
     * load the parameters first, with the SIX BYTES of the three words. These
     * are the bytes SINTRAN really sends: ident 0x01, 5OMDNO 0x08, then zeros,
     * which is the 0x0108 that makes the GIVEINT frame address station 1. */
    body[0] = (uint8_t)NDBUS_ACCP_LSYSPAR;
    body[1] = 0x01u;
    body[2] = 0x08u;
    body[3] = 0x12u;
    body[4] = 0x34u;
    body[5] = 0x56u;
    body[6] = 0x78u;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 7, replies);
    CHECK(nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "data replies: LSYSPAR is acked");
    CHECK(nd.lsyspar_word1 == 0x0108u, "data replies: and word 1 is 0x0108");

    /* RSSYSPAR reads the three words back, each most significant byte first.
     * Reading them back is the only check that the station STORED all three and
     * not just the one the GIVEINT frame needs. */
    body[0] = (uint8_t)NDBUS_ACCP_RSSYSPAR;
    n       = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                        replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 7, "RSSYSPAR answers the ack and three 16-bit words");
    CHECK(plen == 7 && payload[0] == 0x00u && payload[1] == 0x01u && payload[2] == 0x08u &&
              payload[3] == 0x12u && payload[4] == 0x34u && payload[5] == 0x56u &&
              payload[6] == 0x78u,
          "RSSYSPAR reads back exactly the bytes LSYSPAR was given");

    /* RASTS (050B) - ack plus the 16-bit ACCP status word 0x1011. Not issued by
     * MONITOR J04; the bytes are the firmware measurement. */
    body[0] = (uint8_t)NDBUS_ACCP_RASTS;
    n       = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                        replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 3, "RASTS answers three bytes");
    CHECK(plen == 3 && payload[0] == 0x00u && payload[1] == NDBUS_ACCP_ASTS_HIGH &&
              payload[2] == NDBUS_ACCP_ASTS_LOW,
          "RASTS: ack plus the ASTS word 0x1011");

    /* RECO (020B) - ack plus SIXTEEN WORDS from firmware RAM, all zero on a card
     * that has loaded nothing. Thirty-three bytes in all, and the length is the
     * part that bites: at 33 frames plus the envelope the reply is longer than
     * the ND-100 card's 16-word receive FIFO, which is why nd100x needed the
     * busy-retry park before the monitor could read this one whole. All-zero
     * words are what makes MONITOR J04 print "ECO not available". */
    body[0] = (uint8_t)NDBUS_ACCP_RECO;
    n       = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                        replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 33, "RECO answers the ack and sixteen words");
    if (plen == 33)
    {
        int all_zero = 1;
        for (int i = 0; i < 33; i++)
        {
            if (payload[i] != 0x00u)
            {
                all_zero = 0;
            }
        }
        CHECK(all_zero == 1, "RECO: every one of the thirty-three bytes is zero");
    }

    /* PRGMVERS (075B) - ack plus the twelve ASCII bytes of the PROM version.
     * This is the one the monitor renders as "Accp version..: 88.12. 5 I0", and
     * the ASCII is checked as ASCII so a wrong byte names itself. */
    body[0] = (uint8_t)NDBUS_ACCP_PRGMVERS;
    n       = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                        replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 13, "PRGMVERS answers the ack and twelve ASCII bytes");
    if (plen == 13)
    {
        static const char expect[13] = "88.12. 5 I01";
        CHECK(payload[0] == 0x00u, "PRGMVERS: a leading status byte");
        CHECK(memcmp(&payload[1], expect, 12) == 0, "PRGMVERS: the PROM version 88.12. 5 I01");
    }

    /* ALIVE (037B) follows the microprogram flip-flop, and it is answered in the
     * dispatcher rather than by the guard table - the real card reads a hardware
     * alive signal, which is why ndbus_accp_evaluate accepts ALIVE outright.
     * Check both halves, and check the guard still says accepted, or a future
     * change that moves the decision into the table would pass unnoticed. */
    CHECK(ndbus_accp_evaluate(NDBUS_ACCP_ALIVE, &nd.accp) == NDBUS_ACCP_ACCEPTED,
          "ALIVE is never refused by the guard table, running or not");

    /* Start the microprogram first. RESTMIC needs FOUR parameter bytes and a
     * short message is refused in SILENCE, so a two-byte body here would make a
     * correct refusal look like a missing transition. */
    body[0] = (uint8_t)NDBUS_ACCP_RESTMIC;
    body[1] = 0x00u;
    body[2] = 0x00u;
    body[3] = 0x00u;
    body[4] = 0x00u;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 5,
                    replies);
    CHECK(nd.accp.microprogram_running, "ALIVE: the microprogram is running by now");
    body[0] = (uint8_t)NDBUS_ACCP_ALIVE;
    n       = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                        replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 1 && payload[0] == 0x00u, "ALIVE while running is a bare Messack");
    CHECK(nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "and counted as accepted");

    /* STOPMIC clears the flip-flop, so the same command now naks 7. Nak 7 is a
     * LONG Messnak - four bytes, the error code and the ASTS word - because only
     * code 13 has the short form. */
    body[0] = (uint8_t)NDBUS_ACCP_STOPMIC;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                    replies);
    CHECK(!nd.accp.microprogram_running, "ALIVE: STOPMIC clears the microprogram flip-flop");

    body[0] = (uint8_t)NDBUS_ACCP_ALIVE;
    n       = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                        replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(nd.last_nak_code == NDBUS_ACCP_NAK_NOT_ALIVE, "ALIVE while stopped naks 7, not alive");
    CHECK(plen == 4, "and it is the long Messnak, four bytes");
    CHECK(plen == 4 && payload[0] == 0xFFu && payload[1] == 7u &&
              payload[2] == NDBUS_ACCP_ASTS_HIGH && payload[3] == NDBUS_ACCP_ASTS_LOW,
          "ALIVE's nak carries code 7 and the ASTS word");

    /* And a restart brings it back - the flip-flop is a flip-flop, not a latch
     * that only ever falls. */
    body[0] = (uint8_t)NDBUS_ACCP_RESTMIC;
    body[1] = 0x00u;
    body[2] = 0x00u;
    body[3] = 0x00u;
    body[4] = 0x00u;
    (void)send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 5,
                    replies);
    body[0] = (uint8_t)NDBUS_ACCP_ALIVE;
    n       = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1,
                        replies);
    plen = accp_reply_payload(replies, n, NDBUS_STATION_ND5000_FIRST, payload,
                              (int)sizeof(payload));
    CHECK(plen == 1 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED,
          "ALIVE is alive again after RESTMIC");

    ndbus_pool_destroy(&pool);
}

/* -------------------------------------------------------------------------- */
/* Layer 16: the doorbell sniff's latch rules.                                */
/*                                                                            */
/* Ported from RetroCore OctobusDoorbellSniffConfigTests.cs. FOUR of its ten  */
/* cases transfer; the other six do not, and saying which is the point:        */
/*                                                                            */
/*   MinusOneToZero_SelfDiscoversTheMailbox, ImplausibleGeometry_IsRejected,   */
/*   X5actDisplacement_IsTenBytes and the three RequireInit_* cases all assert */
/*   on a GEOMETRY the reference DERIVES from the write address - ext block =  */
/*   address - 0x0A, header = ext - CPUNO*256 - and then sanity-checks against */
/*   the window. This station derives nothing from the sniff. It takes the     */
/*   mailbox base from START_MESS in control-store word 026B at ENKICK, which  */
/*   Layer 12 pins, and the reference's own comment says the same thing: the   */
/*   0xFFFF->0 sniff "REPLACED the old sniff that latched noise". Porting the  */
/*   derivation would be inventing a mechanism this station does not have.     */
/*                                                                            */
/* What DOES transfer is the latch rule itself: which write is a signature,    */
/* how the transitions are counted, and what a threshold does.                */
/* -------------------------------------------------------------------------- */
static void test_doorbell_sniff_rules(void)
{
    printf("Layer 16: the doorbell sniff's latch rules\n");

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the sniff");

    /* Two plausible X5ACT cells, far enough apart that a per-address count and a
     * global one give different answers. */
    const uint32_t cell_a = 0x1000u + 0x0Au;
    const uint32_t cell_b = 0x2000u + 0x0Au;

    /* ---- the signature is the TRANSITION, not the value written ---- */
    {
        NdbusNd5000 nd;
        CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, NULL, NULL),
              "a station for the signature test");
        CHECK(!nd.sniff.latched, "nothing latched before any write");

        /* NotPrecededByMinusOne_DoesNotSelfDiscover: the cell holds 1, which is
         * what the microcode's IDLE_2 re-arms it to, so every doorbell after the
         * first looks like this. Latching on it would accept any zero-write. */
        (void)ndbus_pool_write16(&pool, cell_a, 1u);
        CHECK(!ndbus_nd5000_sniff_before_write16(&nd, cell_a, 0u),
              "1 -> 0 is not an X5ACT signature");
        CHECK(!nd.sniff.latched, "and nothing latched");

        /* A 0xFFFF -> non-zero write is not one either. */
        (void)ndbus_pool_write16(&pool, cell_a, 0xFFFFu);
        CHECK(!ndbus_nd5000_sniff_before_write16(&nd, cell_a, 1u),
              "0xFFFF -> 1 is not a signature: the doorbell rings by writing ZERO");
        CHECK(!nd.sniff.latched, "and nothing latched");

        /* MinusOneToZero: the first doorbell after XMSINIT, which initialises
         * X5ACT to -1. This one latches. */
        CHECK(ndbus_nd5000_sniff_before_write16(&nd, cell_a, 0u),
              "0xFFFF -> 0 is the signature, and it latches");
        CHECK(nd.sniff.latched, "the sniff is latched");
        CHECK(nd.sniff.candidate_offset == cell_a, "on the offset that was written");

        /* Once latched it stays latched and stops answering - a second candidate
         * must not move it. */
        (void)ndbus_pool_write16(&pool, cell_b, 0xFFFFu);
        CHECK(!ndbus_nd5000_sniff_before_write16(&nd, cell_b, 0u),
              "a latched sniff does not re-latch on another cell");
        CHECK(nd.sniff.candidate_offset == cell_a, "and keeps the offset it latched on");

        ndbus_nd5000_destroy(&nd);
    }

    /* ---- SniffRepeat_ZeroOrOne_LatchesImmediately ---- */
    for (uint32_t threshold = 0u; threshold <= 1u; threshold++)
    {
        NdbusNd5000 nd;
        CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, NULL, NULL),
              "a station for the immediate-latch test");
        ndbus_nd5000_set_sniff_threshold(&nd, threshold);
        (void)ndbus_pool_write16(&pool, cell_a, 0xFFFFu);
        CHECK(ndbus_nd5000_sniff_before_write16(&nd, cell_a, 0u),
              "threshold 0 and 1 both mean latch on the first transition");
        CHECK(nd.sniff.latched, "and it is latched");
        ndbus_nd5000_destroy(&nd);
    }

    /* ---- SniffRepeat_TwoWithholdsTheLatchOnASingleTransition, and then
     *      SniffRepeat_LatchesOnceTheThresholdIsReached ----
     *
     * NDBUS_X5ACT_LATCH_ON_FIRST records why a threshold of 2 can never be met by
     * the REAL doorbell: XMSINIT sets X5ACT to -1 once and the microcode re-arms
     * it to 1 thereafter, so there is exactly one 0xFFFF -> 0 transition per
     * XMSINIT. The mechanism still has to count correctly, which is what this
     * asserts - a driver that can produce two such transitions is a test fixture,
     * not the machine. */
    {
        NdbusNd5000 nd;
        CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, NULL, NULL),
              "a station for the threshold test");
        ndbus_nd5000_set_sniff_threshold(&nd, 2u);

        (void)ndbus_pool_write16(&pool, cell_a, 0xFFFFu);
        CHECK(!ndbus_nd5000_sniff_before_write16(&nd, cell_a, 0u),
              "one transition does not meet a threshold of two");
        CHECK(!nd.sniff.latched, "so nothing is latched");
        CHECK(nd.sniff.transitions == 1u, "but the transition was counted");

        (void)ndbus_pool_write16(&pool, cell_a, 0xFFFFu);
        CHECK(ndbus_nd5000_sniff_before_write16(&nd, cell_a, 0u),
              "the second transition at the same cell reaches the threshold");
        CHECK(nd.sniff.latched, "and it latches");
        ndbus_nd5000_destroy(&nd);
    }

    /* ---- SniffRepeat_CountsPerAddress_NotGlobally ---- */
    {
        NdbusNd5000 nd;
        CHECK(ndbus_nd5000_init(&nd, NDBUS_STATION_ND5000_FIRST, &pool, NULL, NULL),
              "a station for the per-address count");
        ndbus_nd5000_set_sniff_threshold(&nd, 2u);

        (void)ndbus_pool_write16(&pool, cell_a, 0xFFFFu);
        CHECK(!ndbus_nd5000_sniff_before_write16(&nd, cell_a, 0u), "one at cell A");
        (void)ndbus_pool_write16(&pool, cell_b, 0xFFFFu);
        CHECK(!ndbus_nd5000_sniff_before_write16(&nd, cell_b, 0u),
              "one at cell B does not complete cell A's pair");
        CHECK(!nd.sniff.latched, "two transitions at two addresses latch nothing");
        CHECK(nd.sniff.candidate_offset == cell_b, "the candidate moved to the newer cell");
        CHECK(nd.sniff.transitions == 1u, "with its own count restarted at one");
        ndbus_nd5000_destroy(&nd);
    }

    ndbus_pool_destroy(&pool);
}

/* -------------------------------------------------------------------------- */
/* Layer 17: the trap-stop record, which is how SINTRAN learns the CPU faulted.*/
/*                                                                            */
/* Ported from RetroCore Nd500MicrocodeServicer.AnswerTrapStop, B30 arm. The   */
/* ND-500 and ND-5000 records are NOT the same layout; only the B30 one is     */
/* asserted here, because that is the generation this station fronts.          */
/* -------------------------------------------------------------------------- */
static void test_trap_stop_record(void)
{
    printf("Layer 17: the B30 trap-stop record\n");

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the trap-stop record");
    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);
    NdbusNd5000 nd;
    mbx_init_structures(&pool);
    mbx_attach(&nd, &pool, &fabric);

    /* --- with no process started, a trap stop is refused and says so --------- */
    CHECK(!ndbus_servicer_answer_trap_stop(&nd.servicer, 1u, NDBUS_TRAP_PAGE_FAULT,
                                          0x08000016u, 0x08012818u, 0x8000000Du, 2u),
          "a trap stop with no message recorded for that process is refused");
    CHECK(nd.servicer.trap_stops_attempted == 1u, "the attempt is counted before the refusal");
    CHECK(nd.servicer.trap_stops_declined == 1u, "and the refusal is counted, not silent");
    CHECK(nd.servicer.trap_stops_posted == 0u, "nothing was written");

    /* --- take a start so the process's message is recorded ------------------- */
    nd.servicer.context_area_base = MBX_BASE + 0x3000u;
    s_start_calls = 0;
    s_start_ctx_byte = 0;
    s_start_take = true;
    (void)ndbus_nd5000_set_process_host(&nd, test_start_process);
    mbx_build_message(&pool, NDBUS_MICFU_START, NDBUS_N5STA_TO_ND500);
    mbx_replay_activation(&pool);
    (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);

    /* A TAKEN start reports "nothing answered" - that is the point of it. */
    CHECK(!ndbus_nd5000_service_mailbox(&nd), "the 23B start is taken, so nothing is answered");
    CHECK(nd.servicer.starts_taken == 1u, "and it is counted as taken");
    CHECK(nd.servicer.process_msg[1] == MBX_MSG,
          "the started process's own message is remembered, indexed by its X5CPU of 1");
    CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_WAITING,
          "and the message is left WAITING, not answered");

    /* --- 46B PAGE FAULT: the TRAP_GEN4 record ------------------------------- */
    const uint32_t pc   = 0x08000016u;   /* measured: the swapper's 11th instruction */
    const uint32_t la   = 0x08012818u;   /* measured: the data address that faulted  */
    const uint32_t mms  = 0xA000000Du;   /* write access, MMWHERE = PFZPST           */
    const uint16_t psn  = 2u;

    CHECK(ndbus_servicer_answer_trap_stop(&nd.servicer, 1u, NDBUS_TRAP_PAGE_FAULT, pc, la, mms,
                                         psn),
          "a 46B page fault is posted on the started process's message");
    CHECK(nd.servicer.trap_stops_posted == 1u, "and counted as posted");
    CHECK(nd.servicer.page_faults_posted == 1u, "and counted as a page fault specifically");

    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_STOPR * 2u) == NDBUS_STOPR_TRAPCODE,
          "STOPR says TRAPCODE - the process stopped on a trap");
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_TRAPN * 2u) == NDBUS_TRAP_PAGE_FAULT,
          "TRAPN carries 46B");

    /* The saved P goes in TWICE: 0o12-0o13 and again 0o14-0o15. A record that
     * wrote it once leaves SINTRAN reading zero from whichever pair it uses. */
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_NUMPA * 2u) == (uint16_t)(pc >> 16),
          "the saved P high half is at 0o12");
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_MCNO * 2u) == (uint16_t)(pc & 0xFFFF),
          "and its low half at 0o13");
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_MSWMC * 2u) == (uint16_t)(pc >> 16),
          "the SAME P appears again at 0o14");
    CHECK(mbx_read(&pool, MBX_MSG + (NDBUS_MSG_MSWMC + 1u) * 2u) == (uint16_t)(pc & 0xFFFF),
          "and at 0o15");

    /* On the B30 the fault logical address is at 0o17-0o20 for EVERY stop trap. */
    CHECK(mbx_read(&pool, MBX_MSG + 0x0Fu * 2u) == (uint16_t)(la >> 16),
          "the fault logical address high half is at 0o17");
    CHECK(mbx_read(&pool, MBX_MSG + 0x10u * 2u) == (uint16_t)(la & 0xFFFF),
          "and its low half at 0o20");

    /* 46B ONLY: physical segment at 0o21, MMS status at 0o22-0o23. */
    CHECK(mbx_read(&pool, MBX_MSG + 0x11u * 2u) == psn,
          "46B puts the physical segment at 0o21");
    CHECK(mbx_read(&pool, MBX_MSG + 0x12u * 2u) == (uint16_t)(mms >> 16),
          "and the MMS status high half at 0o22");
    CHECK(mbx_read(&pool, MBX_MSG + 0x13u * 2u) == (uint16_t)(mms & 0xFFFF),
          "and its low half at 0o23");

    CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ANSWER,
          "and the message is answered in place, which is what wakes SINTRAN");

    /* --- A SECOND FAULT ON THE SAME PROCESS MUST ALSO BE POSTED -------------
     *
     * This is the case a single cleared-on-answer "active message" field gets
     * wrong, and RetroCore measured what it cost: two faults at one PC one byte
     * apart, the first taken and the second REFUSED, and the refusal crashed the
     * CPU because a page fault is classified fatal. The message is remembered per
     * process and never cleared, so the second post lands. */
    CHECK(ndbus_servicer_answer_trap_stop(&nd.servicer, 1u, NDBUS_TRAP_PAGE_FAULT, pc + 1u,
                                         la + 1u, mms, psn),
          "a second fault on the same process is posted, not refused");
    CHECK(nd.servicer.trap_stops_posted == 2u, "both posts are counted");
    CHECK(nd.servicer.trap_stops_declined == 1u, "and nothing was declined the second time");
    CHECK(mbx_read(&pool, MBX_MSG + 0x10u * 2u) == (uint16_t)((la + 1u) & 0xFFFF),
          "the second record overwrote the first with its own fault address");

    /* --- EVERY OTHER STOP TRAP USES THE OTHER LAYOUT ------------------------
     *
     * TRAP_GEN3: the MMS status moves to 0o21-0o22 and the physical segment to
     * 0o25. Asserting this is what stops the two records being merged back into
     * one "identical layout" - the reading that was already corrected once. */
    const uint16_t protect_violation = 0x24u;   /* 44B */
    (void)ndbus_pool_write16(&pool, MBX_MSG + 0x11u * 2u, 0u);
    (void)ndbus_pool_write16(&pool, MBX_MSG + 0x12u * 2u, 0u);
    (void)ndbus_pool_write16(&pool, MBX_MSG + 0x13u * 2u, 0u);
    (void)ndbus_pool_write16(&pool, MBX_MSG + 0x15u * 2u, 0u);

    CHECK(ndbus_servicer_answer_trap_stop(&nd.servicer, 1u, protect_violation, pc, la, mms, psn),
          "a 44B protect violation is posted too");
    CHECK(nd.servicer.page_faults_posted == 2u,
          "but it is NOT counted as a page fault - only 46B is");
    CHECK(mbx_read(&pool, MBX_MSG + 0x11u * 2u) == (uint16_t)(mms >> 16),
          "44B puts the MMS status high half at 0o21, where 46B put the segment");
    CHECK(mbx_read(&pool, MBX_MSG + 0x12u * 2u) == (uint16_t)(mms & 0xFFFF),
          "and its low half at 0o22");
    CHECK(mbx_read(&pool, MBX_MSG + 0x15u * 2u) == psn,
          "and the physical segment at 0o25");
    CHECK(mbx_read(&pool, MBX_MSG + 0x13u * 2u) == 0u,
          "0o23 is left alone - it is the 46B record's slot, not this one's");

    /* The fault logical address is in the same place for both. */
    CHECK(mbx_read(&pool, MBX_MSG + 0x0Fu * 2u) == (uint16_t)(la >> 16),
          "and 0o17-0o20 still carries the fault address, as it does for every stop trap");

    /* An X5CPU past the mailbox's own process count is refused rather than
     * indexing off the end of the array. */
    CHECK(!ndbus_servicer_answer_trap_stop(&nd.servicer, NDBUS_SERVICER_MAX_PROCESSES,
                                          NDBUS_TRAP_PAGE_FAULT, pc, la, mms, psn),
          "an X5CPU outside the process array is refused");

    ndbus_nd5000_destroy(&nd);
    ndbus_pool_destroy(&pool);
}

/* -------------------------------------------------------------------------- */
/* Layer 18: the physical segment table is WORD wide on an ND-5000.           */
/*                                                                            */
/* PHYSRD/PHYSWR carry a physical SEGMENT in MSWMC and an offset inside it, so */
/* the station has to resolve the segment through the same physical segment    */
/* table the MMU walks. The two generations do not agree on its width:         */
/*                                                                            */
/*   ND-500 (3022): HALFWORD entries, mode in bits 15-14, page in 13-0.        */
/*   ND-5000 (B30): WORD entries,     mode in bits 31-30, page in 29-0.        */
/*                                                                            */
/* This station fronts a B30. Reading the table at segment*2 lands inside the  */
/* wrong entry and resolves to a plausible-looking page that belongs to        */
/* something else, so the transfer is performed - into the wrong page.         */
/*                                                                            */
/* MEASURED 30-SEP-2026: with the halfword read, thirteen trap-configuration   */
/* writes SINTRAN aimed at a process control block landed in the swapper's own */
/* data page table instead and zeroed live entries 37-45 and 47-49. The        */
/* swapper then faulted on its fourth instruction, on a page the writes had    */
/* just destroyed, and the monitor reported "The Swapper stopped". With the    */
/* word read the same thirteen writes land in the control block, the page      */
/* table is untouched, and the swapper runs on with no fault at all.           */
/* -------------------------------------------------------------------------- */
static void test_physical_segment_width(void)
{
    printf("Layer 18: the ND-5000 physical segment table is word wide\n");

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, 256 * 1024), "a pool for the segment table");
    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);
    NdbusNd5000 nd;
    mbx_init_structures(&pool);
    mbx_attach(&nd, &pool, &fabric);

    /* A physical segment table with two entries whose WORD and HALFWORD readings
     * name DIFFERENT pages. Entry 1 = 0x400000E6: as a word that is mode 1, page
     * 0x26; the halfword at 1*2 would be the HIGH half of entry 0 instead. Both
     * pages are small enough to lie inside this pool, so the out-of-pool guard
     * cannot be what separates the two readings - only the address can. */
    const uint32_t pst = MBX_BASE + 0x4000u;
    CHECK(ndbus_servicer_set_pst_base(&nd.servicer, pst), "the segment table base is set");

    (void)ndbus_pool_write16(&pool, pst + 0u, 0x0000u);  /* entry 0, high half */
    (void)ndbus_pool_write16(&pool, pst + 2u, 0x0053u);  /* entry 0, low half  */
    (void)ndbus_pool_write16(&pool, pst + 4u, 0x4000u);  /* entry 1, high half */
    (void)ndbus_pool_write16(&pool, pst + 6u, 0x0026u);  /* entry 1, low half  */

    /* Segment 1, offset 0x96 - the shape of the measured trap-config write. */
    const uint32_t offset_in_segment = 0x96u;
    const uint32_t word_page = 0x26u;   /* what entry 1 says, read as a word */
    const uint32_t expect = (word_page * 2048u) + offset_in_segment;

    /* The halfword reading would take the halfword at pst+2, which is entry 0's
     * LOW half, 0x0053 - a different page entirely, and one that exists, so the
     * transfer succeeds and silently writes into the wrong place. */
    const uint32_t halfword_page = 0x53u;
    const uint32_t wrong = (halfword_page * 2048u) + offset_in_segment;
    CHECK(expect != wrong, "the two readings really do name different pages");

    /* Put a recognisable value in the buffer and a sentinel where the WRONG
     * reading would put it, so a regression is caught by the damage it does and
     * not only by the address it misses. */
    const uint32_t buffer = MBX_BASE + 0x6000u;
    (void)ndbus_pool_write16(&pool, buffer, 0x1234u);
    (void)ndbus_pool_write16(&pool, buffer + 2u, 0x5678u);
    (void)ndbus_pool_write16(&pool, wrong, 0xBEEFu);
    (void)ndbus_pool_write16(&pool, wrong + 2u, 0xCAFEu);

    /* 31B PHYSWR: A is segment-relative, B is the buffer, B -> A. */
    mbx_copy_message(&pool, NDBUS_MICFU_PHYSWR, offset_in_segment, buffer, 4u);
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_MSWMC * 2u, 1u); /* segment 1 */

    CHECK(ndbus_nd5000_service_mailbox(&nd), "31B PHYSWR is serviced");
    CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ANSWER, "and answered");
    CHECK(nd.servicer.segment_resolved == 1u, "the segment resolved through the table");
    CHECK(nd.servicer.segment_unresolved == 0u, "and did not fall back to a flat address");

    CHECK(ndbus_pool_read16(&pool, expect) == 0x1234u,
          "the transfer landed in the page the WORD entry names");
    CHECK(ndbus_pool_read16(&pool, expect + 2u) == 0x5678u, "both halfwords of it");

    CHECK(ndbus_pool_read16(&pool, wrong) == 0xBEEFu,
          "and the page the HALFWORD reading would have named is untouched - reading "
          "this table at segment*2 corrupts whatever lives there");
    CHECK(ndbus_pool_read16(&pool, wrong + 2u) == 0xCAFEu, "sentinel intact");

    /* THE PAGE FIELD IS 30 BITS, NOT 14. Masking an ND-5000 entry with the
     * ND-500's 0x3FFF folds a large page number down to a small one that still
     * looks like a valid page, so the transfer succeeds into the wrong place
     * rather than failing where it would be noticed. */
    (void)ndbus_pool_write16(&pool, pst + 8u, 0x0001u);   /* entry 2 = 0x0001C000 */
    (void)ndbus_pool_write16(&pool, pst + 10u, 0xC000u);
    mbx_copy_message(&pool, NDBUS_MICFU_PHYSWR, 0u, buffer, 2u);
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_MSWMC * 2u, 2u);

    /* 0x1C000 pages is far outside a 256 KB pool, so the guard must refuse the
     * transfer. Under the 14-bit mask the page would read as 0x0000, which the
     * resolver reports as "not present" - a different answer that happens to also
     * refuse, so assert the COUNTER that says which path was taken. */
    (void)ndbus_nd5000_service_mailbox(&nd);
    CHECK(nd.servicer.segment_resolved == 2u,
          "a 30-bit page number resolves rather than masking down to zero");
    CHECK(nd.servicer.copies_refused == 1u,
          "and the out-of-pool guard refuses it, instead of a folded page succeeding");

    ndbus_nd5000_destroy(&nd);
    ndbus_pool_destroy(&pool);
}

/* -------------------------------------------------------------------------- */
/* Layer 19: the monitor-call record.                                         */
/*                                                                            */
/* An ND-500 monitor call is NOT served on the ND-500. The program executes    */
/* its call, the process stops, the record goes to SINTRAN on the ND-100, and  */
/* SINTRAN restarts the process with 3MONCO (24B). Ported from RetroCore       */
/* Nd500MicrocodeServicer.AnswerMonitorCallStop.                              */
/* -------------------------------------------------------------------------- */
static void test_monitor_call_record(void)
{
    printf("Layer 19: the monitor-call record\n");

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the monitor-call record");
    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);
    NdbusNd5000 nd;
    mbx_init_structures(&pool);
    mbx_attach(&nd, &pool, &fabric);

    /* Refused before any process is started, and counted rather than passed over. */
    CHECK(!ndbus_servicer_answer_monitor_call(&nd.servicer, 1u, 0x08008255u, 0x28u, 4u, NULL,
                                             NULL),
          "a monitor call with no message recorded for that process is refused");
    CHECK(nd.servicer.mon_calls_attempted == 1u, "the attempt is counted before the refusal");
    CHECK(nd.servicer.mon_calls_declined == 1u, "and the refusal is counted");
    CHECK(nd.servicer.mon_calls_posted == 0u, "nothing was written");

    /* Start a process so its message is remembered. */
    nd.servicer.context_area_base = MBX_BASE + 0x3000u;
    s_start_calls = 0;
    s_start_ctx_byte = 0;
    s_start_take = true;
    (void)ndbus_nd5000_set_process_host(&nd, test_start_process);
    mbx_build_message(&pool, NDBUS_MICFU_START, NDBUS_N5STA_TO_ND500);
    mbx_replay_activation(&pool);
    (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);
    CHECK(!ndbus_nd5000_service_mailbox(&nd), "the start is taken");

    /* MON 50B with four arguments - the shape RetroCore measured on a real call. */
    const uint32_t saved_p = 0x08008255u;
    const uint16_t mon     = 0x28u;              /* 50B */
    const uint32_t addrs[4] = { 0x08001000u, 0x08001004u, 0u, 0x0800200Cu };
    const uint32_t vals[4]  = { 0x11112222u, 0x33334444u, 0u, 0x55556666u };

    CHECK(ndbus_servicer_answer_monitor_call(&nd.servicer, 1u, saved_p, mon, 4u, addrs, vals),
          "MON 50B is posted on the started process's message");
    CHECK(nd.servicer.mon_calls_posted == 1u, "and counted");
    CHECK(nd.servicer.last_mon_number == mon, "the monitor number is remembered");

    /* STOPR says MOCALL(1), NOT TRAPCODE(2) - the two stops are told apart by this
     * halfword alone, and SINTRAN runs completely different code for each. */
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_STOPR * 2u) == NDBUS_STOPR_MOCALL,
          "STOPR says MOCALL, which is 1 - not TRAPCODE, which is 2");
    CHECK(NDBUS_STOPR_MOCALL != NDBUS_STOPR_TRAPCODE, "and the two really do differ");
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_NUMPA * 2u) == 4u, "NUMPA carries the count");
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_MCNO * 2u) == mon, "MCNO carries the number");

    /* The saved P shares the halfword pair the copy family calls addrA. */
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_N500A * 2u) == (uint16_t)(saved_p >> 16),
          "the saved P high half is at 0o7");
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_SWRST * 2u) == (uint16_t)(saved_p & 0xFFFF),
          "and its low half at 0o10");

    /* Addresses at 0o40 + 2k, values at 0o100 + 2k, both 32-bit. */
    for (uint32_t k = 0; k < 4u; k++)
    {
        uint32_t as = MBX_MSG + 0x40u + 4u * k;
        uint32_t vs = MBX_MSG + 0x80u + 4u * k;
        CHECK(mbx_read(&pool, as) == (uint16_t)(addrs[k] >> 16), "argument address high half");
        CHECK(mbx_read(&pool, as + 2u) == (uint16_t)(addrs[k] & 0xFFFF), "and its low half");
        CHECK(mbx_read(&pool, vs) == (uint16_t)(vals[k] >> 16), "argument value high half");
        CHECK(mbx_read(&pool, vs + 2u) == (uint16_t)(vals[k] & 0xFFFF), "and its low half");
    }

    /* The two slot runs must not overlap: 0x40 + 4*15 = 0x7C, the last address
     * slot, and the first value slot is 0x80. A stride of 8 or a base of 0x60
     * would have the value of one argument land on the address of another. */
    CHECK(NDBUS_MON_ARG_ADDR_BASE + 4u * (NDBUS_MON_MAX_ARGS - 1u) < NDBUS_MON_ARG_VALUE_BASE,
          "sixteen address slots fit below the first value slot");

    CHECK(mbx_msg_status(&pool) == NDBUS_N5STA_ANSWER, "and the message is answered in place");

    /* THE COUNT IS CLAMPED TO THE MICROCODE'S SIXTEEN SLOTS. CALL_MON checks it,
     * and a larger count here would write past the value slots into whatever
     * follows them in the message. */
    uint32_t big_addrs[NDBUS_MON_MAX_ARGS];
    uint32_t big_vals[NDBUS_MON_MAX_ARGS];
    for (uint32_t k = 0; k < NDBUS_MON_MAX_ARGS; k++)
    {
        big_addrs[k] = 0xAA000000u + k;
        big_vals[k]  = 0xBB000000u + k;
    }
    const uint32_t past = MBX_MSG + 0x80u + 4u * NDBUS_MON_MAX_ARGS;
    (void)ndbus_pool_write16(&pool, past, 0xFEEDu);

    CHECK(ndbus_servicer_answer_monitor_call(&nd.servicer, 1u, saved_p, mon, 99u, big_addrs,
                                            big_vals),
          "a count of 99 is accepted");
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_NUMPA * 2u) == NDBUS_MON_MAX_ARGS,
          "but reported as sixteen - the microcode's own slot limit");
    CHECK(mbx_read(&pool, past) == 0xFEEDu,
          "and nothing was written past the sixteenth value slot");

    /* NULL argument arrays are a zero-filled record, not a crash - a monitor call
     * with no arguments is ordinary. */
    CHECK(ndbus_servicer_answer_monitor_call(&nd.servicer, 1u, saved_p, 0x03u, 0u, NULL, NULL),
          "MON 3B with no arguments is posted");
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_NUMPA * 2u) == 0u, "NUMPA is zero");
    CHECK(mbx_read(&pool, MBX_MSG + NDBUS_MSG_MCNO * 2u) == 0x03u, "and MCNO is the number");

    ndbus_nd5000_destroy(&nd);
    ndbus_pool_destroy(&pool);
}

/* -------------------------------------------------------------------------- */
/* Layer 20: the 3MONCO answer, whose slots mean something ELSE inbound.      */
/*                                                                            */
/* Decoded from the B30 write-back loop MSG_CONMC 015734-015751. Three slots   */
/* are re-used for the answer, so reading them as their outbound meanings gets */
/* all three wrong:                                                           */
/*                                                                            */
/*   MCNO + MSWMC  outbound: the monitor number and half the saved P           */
/*                 inbound:  FUNCV, one 32-bit result, into the process's I1   */
/*   STOPR         outbound: MOCALL or TRAPCODE                               */
/*                 inbound:  KFLIP, which sets or clears the K flag           */
/*   NUMPA         outbound: the argument COUNT                               */
/*                 inbound:  a write-back MASK, bit k naming parameter k       */
/* -------------------------------------------------------------------------- */
static void test_monitor_call_result(void)
{
    printf("Layer 20: the 3MONCO answer and its write-back mask\n");

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the 3MONCO answer");
    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);
    NdbusNd5000 nd;
    mbx_init_structures(&pool);
    mbx_attach(&nd, &pool, &fabric);

    NdbusMonResult r;
    CHECK(!ndbus_servicer_read_monitor_result(&nd.servicer, 0u, &r),
          "a zero message address is refused");
    CHECK(!ndbus_servicer_read_monitor_result(&nd.servicer, MBX_MSG, NULL),
          "and so is a NULL result");

    /* Build an answer the way SINTRAN does. FUNCV spans two slots that carried
     * completely different things outbound. */
    const uint32_t funcv = 0x0000002Au;
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_MCNO * 2u, (uint16_t)(funcv >> 16));
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_MSWMC * 2u, (uint16_t)(funcv & 0xFFFF));

    /* KFLIP non-zero: the call failed and the program will branch on K. */
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_STOPR * 2u, 1u);

    /* A MASK with bits 0 and 3 set - NOT a count of two, and not four parameters.
     * Reading it as a count would write back parameters 0 and 1 and miss 3. */
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_NUMPA * 2u, 0x0009u);

    const uint32_t addrs[4] = { 0x08001000u, 0x08001004u, 0x08001008u, 0x0800100Cu };
    const uint32_t vals[4]  = { 0xAAAA1111u, 0xBBBB2222u, 0xCCCC3333u, 0xDDDD4444u };
    for (uint32_t k = 0; k < 4u; k++)
    {
        uint32_t as = MBX_MSG + 0x40u + 4u * k;
        uint32_t vs = MBX_MSG + 0x80u + 4u * k;
        (void)ndbus_pool_write16(&pool, as, (uint16_t)(addrs[k] >> 16));
        (void)ndbus_pool_write16(&pool, as + 2u, (uint16_t)(addrs[k] & 0xFFFF));
        (void)ndbus_pool_write16(&pool, vs, (uint16_t)(vals[k] >> 16));
        (void)ndbus_pool_write16(&pool, vs + 2u, (uint16_t)(vals[k] & 0xFFFF));
    }

    CHECK(ndbus_servicer_read_monitor_result(&nd.servicer, MBX_MSG, &r), "the answer is read");
    CHECK(nd.servicer.mon_results_read == 1u, "and counted");

    CHECK(r.funcv == funcv, "FUNCV is the 32-bit value spanning MCNO and MSWMC");
    CHECK(r.kflip == 1u, "KFLIP comes out of the STOPR slot");
    CHECK(r.mask == 0x0009u, "NUMPA is taken as a mask");

    /* THE MASK SELECTS, IT DOES NOT COUNT. Bits 0 and 3, so two write-backs, and
     * the SECOND one is parameter 3 - not parameter 1. A count-based reading gives
     * the right NUMBER of write-backs and the wrong addresses, which is the kind of
     * wrong answer that looks plausible in a log. */
    CHECK(r.count == 2u, "two bits set means two write-backs");
    CHECK(r.addresses[0] == addrs[0], "the first is parameter 0's address");
    CHECK(r.values[0] == vals[0], "with parameter 0's value");
    CHECK(r.addresses[1] == addrs[3],
          "the second is parameter 3's address, NOT parameter 1's - the mask selects");
    CHECK(r.values[1] == vals[3], "with parameter 3's value");

    /* An empty mask is a legitimate answer: a call that returns only FUNCV. */
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_NUMPA * 2u, 0u);
    CHECK(ndbus_servicer_read_monitor_result(&nd.servicer, MBX_MSG, &r), "an empty mask reads");
    CHECK(r.count == 0u, "and selects nothing to write back");
    CHECK(r.funcv == funcv, "while FUNCV still comes through");

    /* KFLIP zero is the success case, and must be distinguishable from the failure
     * one - the whole point of the flag. */
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_STOPR * 2u, 0u);
    CHECK(ndbus_servicer_read_monitor_result(&nd.servicer, MBX_MSG, &r), "and reads again");
    CHECK(r.kflip == 0u, "KFLIP zero is the success case");

    /* Every bit of the mask is honoured, up to the sixteen slots that exist. */
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_NUMPA * 2u, 0xFFFFu);
    CHECK(ndbus_servicer_read_monitor_result(&nd.servicer, MBX_MSG, &r), "a full mask reads");
    CHECK(r.count == NDBUS_MON_MAX_ARGS,
          "all sixteen bits select, and none beyond - there are only sixteen slots");

    /* ---- 26B 3WMONCO: THE ANSWER-DATA BLOCK ----------------------------------
     *
     * 26B is the 24B restart PLUS a bounded copy of answer data into the
     * process's memory. It was missing entirely, so it fell to the servicer's
     * default arm and was answered 5ERANSWER - the process was never resumed and
     * the answer buffer never delivered. SINTRAN does send it:
     * MP-P2-N500.NPL:2401 selects 3WMONCO over 3MONCO for XMSG functions 6 and
     * 51, and :135641 stages the restart by moving SM26N/SM26A into 26NRB/26ADD.
     *
     * Offsets from the reference's decode of the microcode at 015752-016004:
     * 26NRB at halfword 0o17, 26ADD at 0o15-0o16, ABUFA at 0o140-0o141. ABUFA is
     * a WORD address, shifted left to reach bytes. */
    /* ---- THE VECTORED DISPATCH: strip, range check, then index --------------
     *
     * The microcode does not compare MICFU against a list. It strips bit 15 - a
     * FLAG, not part of the function number - range checks the rest against
     * 0..77B and indexes a 64-entry table. Both emulators switched on the raw
     * halfword and neither range checked, so a flagged message the hardware
     * dispatches normally was answered 5ERANSWER by both. The carve outranks
     * both emulators, so this is a shared defect rather than a port gap:
     * ACCP-OCTOBUS-COMMAND-TABLE-2026-08-02.md, and the mailbox catalogue's
     * read order "N5STA check -> CPU-target check -> MICFU -> vectored dispatch". */
    CHECK(ndbus_micfu_dispatch_code(NDBUS_MICFU_MONCO) == NDBUS_MICFU_MONCO,
          "an ordinary code dispatches through itself");
    CHECK(ndbus_micfu_dispatch_code((uint16_t)(NDBUS_MICFU_MONCO | 0x8000u)) ==
              NDBUS_MICFU_MONCO,
          "and bit 15 is STRIPPED - it is a flag, so a flagged 24B is still a 24B");
    CHECK(ndbus_micfu_is_continue((uint16_t)(NDBUS_MICFU_MONCO | 0x8000u)),
          "so a flagged continue is still classed as a continue, not answered 5ERANSWER");
    CHECK(ndbus_micfu_is_continue((uint16_t)(NDBUS_MICFU_WMONCO | 0x8000u)),
          "and a flagged 26B likewise");
    CHECK(ndbus_micfu_dispatch_code(64u) == 64u,
          "a code at the table's bound is OUT of range - 0..77B is 0..63");
    CHECK(ndbus_micfu_dispatch_code(0x7FFFu) == 64u,
          "and so is anything above it, once the flag is off");
    CHECK(ndbus_micfu_class(64u) == NDBUS_MICFU_CLASS_NONE,
          "an out-of-range code has no class - it is not a function at all");
    CHECK(!ndbus_micfu_is_continue(64u) && !ndbus_micfu_is_start_class(64u),
          "and is neither a continue nor a start, so it cannot reach a handler");

    /* The class table is the SINGLE statement of what each function is. Three
     * hand-written lists of the same codes - is_continue, is_start_class and the
     * dispatch switch - is how 26B came to be missing from all three. */
    CHECK(ndbus_micfu_class(NDBUS_MICFU_RMICV) == NDBUS_MICFU_CLASS_INLINE,
          "3RMICV is answered inline, leaving the process alone");
    CHECK(ndbus_micfu_class(NDBUS_MICFU_START) == NDBUS_MICFU_CLASS_START,
          "3START loads a context block");
    CHECK(ndbus_micfu_class(NDBUS_MICFU_TRACO) == NDBUS_MICFU_CLASS_CONTINUE,
          "3TRACO resumes in place");
    CHECK(ndbus_micfu_is_start_class(NDBUS_MICFU_TRACO),
          "and a continue is start-CLASS as well - it arrives on the same path");
    CHECK(!ndbus_micfu_is_start_class(NDBUS_MICFU_RMICV),
          "while an inline answer is not, or it would try to start a process");

    /* ---- THE COMPLETE DISPATCH, ENUMERATED AGAINST THE ORACLE ---------------
     *
     * The oracle dispatches 22 micro-functions. This pins which of them we serve
     * and which we refuse ON PURPOSE, because the two were conflated once and it
     * cost real time: an audit listed 05, 16B, 17B, 20B, 27B, 34B and 44B as
     * "missing", and FIVE of those seven are gated on `Generation == ND500` in
     * the reference as well - they are MSG_ILLEG in both B30 listings and SINTRAN
     * never transmits them on this generation. Refusing them IS the correct
     * answer here, and only 34B was a real gap.
     *
     * Asserting the refusals, not just the implementations, is the point: a later
     * reader "fixing" a deliberate refusal would make this emulator accept a
     * message the hardware rejects. */
    CHECK(ndbus_micfu_class(NDBUS_MICFU_IMEMRD) == NDBUS_MICFU_CLASS_INLINE,
          "34B IMEMRD is served - it is the B30's instruction-memory READ");
    CHECK(ndbus_micfu_class(NDBUS_MICFU_IMEMWR) == NDBUS_MICFU_CLASS_INLINE,
          "and 35B IMEMWR, its write counterpart - serving one and not the other is "
          "what makes a verify-after-load fail with nothing obviously wrong");
    CHECK(ndbus_micfu_class(NDBUS_MICFU_SWMESS) == NDBUS_MICFU_CLASS_NONE,
          "05 3SWMESS is refused - MSG_ILLEG on the B30, and the reference gates it "
          "on the ND-500 generation too");
    CHECK(ndbus_micfu_class(NDBUS_MICFU_WREG) == NDBUS_MICFU_CLASS_NONE,
          "21B 3WREG likewise - there is no register image on this generation");
    CHECK(ndbus_micfu_class(NDBUS_MICFU_RPREG) == NDBUS_MICFU_CLASS_NONE,
          "44B 3RPREG is refused rather than answered OK-with-no-write: the "
          "reference answers success there and its own comment says what reaches "
          "the message is NOT carved, so success would be a guessed answer");

    /* The names exist so a log line reads; a missing one must not be NULL. */
    CHECK(ndbus_micfu_name(NDBUS_MICFU_WMONCO) != NULL &&
          ndbus_micfu_name(NDBUS_MICFU_WMONCO)[0] == '3',
          "a known function names itself");
    CHECK(ndbus_micfu_name(64u) != NULL && ndbus_micfu_name(0x7FFFu) != NULL,
          "and an out-of-range one still returns a string, never NULL");

    CHECK(ndbus_micfu_is_continue(NDBUS_MICFU_WMONCO),
          "26B is a CONTINUE - classed otherwise it is answered 5ERANSWER and the "
          "process never resumes");
    CHECK(ndbus_micfu_is_start_class(NDBUS_MICFU_WMONCO), "and therefore start-class");

    {
        /* Source bytes somewhere the ND-100 half of the pool can hold them, at a
         * WORD address so ABUFA can name it. */
        const uint32_t src_byte = 0x00002000u;   /* inside this layer's 64 KB pool */
        const uint32_t src_word = src_byte >> 1;
        static const char payload[] = "SYSTEM";   /* 6 bytes, deliberately odd-length */
        for (uint32_t i = 0; i < 6u; i++)
        {
            (void)ndbus_pool_write8(&pool, src_byte + i, (uint8_t)payload[i]);
        }

        (void)ndbus_pool_write16(&pool, MBX_MSG + 15u * 2u, 6u);              /* 26NRB */
        (void)ndbus_pool_write16(&pool, MBX_MSG + 13u * 2u, 0x0800u);         /* 26ADD hi */
        (void)ndbus_pool_write16(&pool, MBX_MSG + 14u * 2u, 0x1234u);         /* 26ADD lo */
        (void)ndbus_pool_write16(&pool, MBX_MSG + 96u * 2u,
                                 (uint16_t)(src_word >> 16));                 /* ABUFA hi */
        (void)ndbus_pool_write16(&pool, MBX_MSG + 97u * 2u,
                                 (uint16_t)(src_word & 0xFFFFu));             /* ABUFA lo */

        NdbusWmoncoBlock wb;
        CHECK(ndbus_servicer_read_wmonco_block(&nd.servicer, MBX_MSG, &wb),
              "a 26B answer-data block is located");
        CHECK(wb.count == 6u, "the byte count comes from 26NRB at halfword 0o17");
        CHECK(wb.dest == 0x08001234u,
              "and the destination from 26ADD, high halfword FIRST");
        CHECK(!wb.oversize, "six bytes is not oversize");
        CHECK(wb.src_byte == src_byte, "and the source from ABUFA, a WORD address shifted");
        {
            bool in_order = true;
            for (uint32_t i = 0; i < 6u; i++)
            {
                if (ndbus_servicer_read_nd100_byte(&nd.servicer, wb.src_byte + i) !=
                    (uint8_t)payload[i]) { in_order = false; }
            }
            CHECK(in_order,
                  "and the bytes stream out in order, odd length and all - a byte-order "
                  "fault here is exactly how a user name comes out interleaved");
        }

        /* THE OVERSIZE CASE IS NOT AN ERROR ANSWER. A count of 0x2000 or more
         * skips the copy and STILL resumes the process, with FUNCV forced to
         * 0o174 and K set. Declining the message instead is what leaves a process
         * parked forever, which is the defect this whole arm exists to avoid. */
        (void)ndbus_pool_write16(&pool, MBX_MSG + 15u * 2u, 0x2000u);
        CHECK(ndbus_servicer_read_wmonco_block(&nd.servicer, MBX_MSG, &wb),
              "an oversize 26B still READS - it is not a refusal");
        CHECK(wb.oversize, "it is flagged oversize");
        CHECK(wb.count == 0u, "so the copy is skipped");

        /* A zero count is the ordinary no-data case and must not look oversize. */
        (void)ndbus_pool_write16(&pool, MBX_MSG + 15u * 2u, 0u);
        CHECK(ndbus_servicer_read_wmonco_block(&nd.servicer, MBX_MSG, &wb),
              "a 26B with no data reads");
        CHECK(wb.count == 0u && !wb.oversize,
              "nothing to copy, and NOT the oversize case - zero is not 0x2000");
    }

    ndbus_nd5000_destroy(&nd);
    ndbus_pool_destroy(&pool);
}


/* ----- the inline user buffer, MON 504B/511B/512B -------------------------- */

/** What the fake ND-500 data reader hands back, and how it was asked. */
static uint8_t  s_inline_src[NDBUS_MON_INLINE_MAX_BYTES];
static uint32_t s_inline_asked_addr;
static uint32_t s_inline_asked_count;
static int      s_inline_calls;
static bool     s_inline_reader_ok;

static bool test_read_nd500_data_bytes(void *ctx, uint32_t logical_address,
                                       uint8_t *destination, uint32_t count)
{
    (void)ctx;
    s_inline_calls++;
    s_inline_asked_addr  = logical_address;
    s_inline_asked_count = count;
    if (!s_inline_reader_ok || count > NDBUS_MON_INLINE_MAX_BYTES)
    {
        return false;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        destination[i] = s_inline_src[i];
    }
    return true;
}

/* ----- the answer slots are a union; the arm follows the message kind ----- */

static void test_stop_kind_selects_the_arm(void)
{
    printf("Layer 25: a restart after a TRAP must not be read as a monitor-call answer\n");

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the stop-kind test");
    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);
    NdbusNd5000 nd;
    mbx_init_structures(&pool);
    mbx_attach(&nd, &pool, &fabric);

    /* Nothing has stopped, so no arm applies. A default of "monitor call" here
     * would decode whatever happens to be in the slots at start-up. */
    CHECK(!ndbus_servicer_stop_was_monitor_call(&nd.servicer, 0u),
          "before any stop, no process carries a monitor-call answer");
    CHECK(!ndbus_servicer_stop_was_monitor_call(&nd.servicer, 1u),
          "and that holds for every process, not just the first");

    nd.servicer.context_area_base = MBX_BASE + 0x3000u;
    s_start_calls = 0;
    s_start_ctx_byte = 0;
    s_start_take = true;
    (void)ndbus_nd5000_set_process_host(&nd, test_start_process);
    mbx_build_message(&pool, NDBUS_MICFU_START, NDBUS_N5STA_TO_ND500);
    mbx_replay_activation(&pool);
    (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);
    CHECK(!ndbus_nd5000_service_mailbox(&nd), "a process is started");

    /* A MONITOR-CALL stop selects the monitor-call arm. */
    uint32_t addrs[2] = { 0x08001000u, 0x08001004u };
    uint32_t vals[2]  = { 0x11112222u, 0x33334444u };
    CHECK(ndbus_servicer_answer_monitor_call(&nd.servicer, 1u, 0x08008255u, 0x28u, 2u,
                                             addrs, vals),
          "a monitor-call stop is recorded");
    CHECK(ndbus_servicer_stop_was_monitor_call(&nd.servicer, 1u),
          "and the process now carries a monitor-call answer");

    /* A TRAP stop over the SAME message takes the arm away again. This is the
     * ordering that bites: the trap record goes into the very halfwords the
     * answer uses, so a reader that does not ask the kind decodes the record. */
    CHECK(ndbus_servicer_answer_trap_stop(&nd.servicer, 1u, 0x26u, 0x0800467Fu, 0x00000004u, 0xA000000Du, 13u),
          "a trap stop is recorded on the same process");
    CHECK(!ndbus_servicer_stop_was_monitor_call(&nd.servicer, 1u),
          "and the monitor-call arm NO LONGER applies - reading it here would take "
          "K from the TRAPCODE value and FUNCV from the trapping P");

    /* THE SLOTS REALLY DO COLLIDE - proven, not asserted. After the trap stop the
     * monitor-call reader still "succeeds" and returns values derived from the
     * trap record, which is exactly why the kind has to be consulted. */
    {
        NdbusMonResult res;
        bool read_ok = ndbus_servicer_read_monitor_result(&nd.servicer, MBX_MSG, &res);
        CHECK(read_ok,
              "the monitor-call reader still succeeds over a trap record - it cannot "
              "tell, which is the whole reason for the kind");
        if (read_ok)
        {
            CHECK(res.kflip == NDBUS_STOPR_TRAPCODE,
                  "and it reads K from the STOPR slot, which a trap stop set to "
                  "TRAPCODE - a non-zero K the process never earned");
        }
    }

    /* And a monitor-call stop after the trap restores the arm, so the gate is not
     * a one-way latch that would silence every later answer. */
    CHECK(ndbus_servicer_answer_monitor_call(&nd.servicer, 1u, 0x08008255u, 0x28u, 2u,
                                             addrs, vals),
          "a later monitor-call stop is recorded");
    CHECK(ndbus_servicer_stop_was_monitor_call(&nd.servicer, 1u),
          "and the arm applies again - the gate is not a latch");

    /* An out-of-range process is refused rather than indexing past the array. */
    CHECK(!ndbus_servicer_stop_was_monitor_call(&nd.servicer, NDBUS_SERVICER_MAX_PROCESSES),
          "an out-of-range process number is refused");
    CHECK(!ndbus_servicer_stop_was_monitor_call(NULL, 0u), "and a NULL servicer");

    ndbus_nd5000_destroy(&nd);
    ndbus_pool_destroy(&pool);
}

static void test_inline_user_buffer(void)
{
    printf("Layer 23: the inline user buffer for the output monitor calls\n");

    /* THE SET IS ASSERTED EXHAUSTIVELY, NOT SAMPLED. The failure this guards
     * against is someone adding a MON number because it sits next to one that is
     * in the set - which is how 513B gets added, and 513B shares only SINTRAN's
     * handler body, never a microcode obligation. A sweep fails on any addition;
     * three spot checks do not. */
    {
        unsigned in_set = 0;
        bool only_the_three = true;
        for (uint32_t m = 0; m <= 0x1FFu; m++)
        {
            bool want = (m == NDBUS_MON_504B_NOUTS)
                     || (m == NDBUS_MON_511B_DVIO)
                     || (m == NDBUS_MON_512B_A5XMSG);
            bool got = ndbus_mon_requires_inline_copy((uint16_t)m);
            if (got) { in_set++; }
            if (got != want) { only_the_three = false; }
        }
        CHECK(in_set == 3u, "exactly three monitor calls inline-copy their buffer");
        CHECK(only_the_three,
              "and they are 504B, 511B and 512B - the microcode's CALL_5XX set, nothing "
              "adjacent to them");
        CHECK(!ndbus_mon_requires_inline_copy(0x14Bu),
              "513B is NOT in the set: it shares SINTRAN's handler body with 512B, and "
              "sharing a handler is not sharing a microcode obligation");
    }

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the inline buffer");
    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);
    NdbusNd5000 nd;
    mbx_init_structures(&pool);
    mbx_attach(&nd, &pool, &fabric);

    /* ABUFA names a WORD address. Point it at a byte address this pool holds. */
    const uint32_t target_byte = 0x00003000u;
    const uint32_t target_word = target_byte >> 1;
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MON_ABUFA_WORD * 2u,
                             (uint16_t)(target_word >> 16));
    (void)ndbus_pool_write16(&pool, MBX_MSG + (NDBUS_MON_ABUFA_WORD + 1u) * 2u,
                             (uint16_t)(target_word & 0xFFFFu));

    CHECK(ndbus_servicer_inline_buffer_target(&nd.servicer, MBX_MSG) == target_byte,
          "ABUFA resolves by SHIFTING - read as a flat byte offset it lands two "
          "megabytes outside the window and the program prints stale bytes");

    /* ODD LENGTH ON PURPOSE. Nineteen is the length a real run showed
     * ("CPU type         : "), and an odd count is where a parity fault hides:
     * the tail halfword has only one real byte in it. */
    static const char text[] = "CPU type         : ";
    const uint32_t n = (uint32_t)(sizeof text - 1u);   /* 19 */
    CHECK(n == 19u && (n & 1u) == 1u, "the sample is 19 bytes, an odd count");

    CHECK(ndbus_servicer_write_inline_buffer(&nd.servicer, MBX_MSG,
                                             (const uint8_t *)text, n),
          "an inline buffer of 19 bytes is written");

    /* EVERY BYTE, BY INDEX. Not a string compare and not a search of a log: a
     * compare that stops at the first difference, or one that looks for a
     * substring, is how an every-other-byte fault gets reported as a pass. */
    {
        uint32_t wrong = 0;
        uint32_t first_wrong = 0xFFFFFFFFu;
        for (uint32_t i = 0; i < n; i++)
        {
            uint8_t got = ndbus_pool_read8(&pool, target_byte + i);
            if (got != (uint8_t)text[i])
            {
                wrong++;
                if (first_wrong == 0xFFFFFFFFu) { first_wrong = i; }
            }
        }
        if (wrong != 0u)
        {
            printf("    %u of %u byte(s) wrong, first at index %u: wanted 0x%02X got 0x%02X\n",
                   (unsigned)wrong, (unsigned)n, (unsigned)first_wrong,
                   (unsigned)(uint8_t)text[first_wrong],
                   (unsigned)ndbus_pool_read8(&pool, target_byte + first_wrong));
        }
        CHECK(wrong == 0u,
              "all 19 bytes land in order - a pair swap here is exactly how a user name "
              "comes out interleaved");
    }

    /* The odd final byte pairs with ZERO. The alternative is reading one byte past
     * the caller's buffer, which a sanitizer catches and a release build does not. */
    CHECK(ndbus_pool_read8(&pool, target_byte + n) == 0x00u,
          "the odd tail byte pairs with zero, not with whatever follows the source");

    /* WSMC is what makes SINTRAN read the message instead of asking for the buffer.
     * Set explicitly, because a bit that happened to be set already is what made
     * this fail silently rather than loudly. */
    {
        uint16_t miflag = ndbus_pool_read16(&pool, MBX_MSG - NDBUS_MON_MIFLAG_BACK_BYTES);
        CHECK((miflag & NDBUS_MON_MIFLAG_WSMC) != 0u,
              "MIFLAG bit WSMC is set, 16 bytes BEFORE the message base");
    }

    /* BOTH GUARDS ARE THE MICROCODE'S, and both mean COPY NOTHING. The over-size
     * one is the trap: clamping and copying a prefix is the "helpful" change, and
     * it is a divergence. Prove it by leaving a witness byte and checking it
     * survives. */
    (void)ndbus_pool_write8(&pool, target_byte, 0xA5u);
    CHECK(!ndbus_servicer_write_inline_buffer(&nd.servicer, MBX_MSG,
                                              (const uint8_t *)text, 0u),
          "a count of zero copies nothing - guard 010716");
    CHECK(ndbus_pool_read8(&pool, target_byte) == 0xA5u, "and the buffer is untouched");

    CHECK(!ndbus_servicer_write_inline_buffer(&nd.servicer, MBX_MSG,
                                              (const uint8_t *)text,
                                              NDBUS_MON_INLINE_MAX_BYTES + 1u),
          "a count over 0o4000 copies nothing - guard 010717");
    CHECK(ndbus_pool_read8(&pool, target_byte) == 0xA5u,
          "and it copies NOTHING, not a truncated prefix - a clamp here would be a "
          "divergence from the microcode");

    CHECK(ndbus_servicer_write_inline_buffer(&nd.servicer, MBX_MSG,
                                             (const uint8_t *)text,
                                             1024u),
          "a count well inside the limit is accepted");

    /* ABUFA ZERO MEANS NO BUFFER. Writing to pool offset 0 would corrupt the base
     * of the window instead of declining. */
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MON_ABUFA_WORD * 2u, 0u);
    (void)ndbus_pool_write16(&pool, MBX_MSG + (NDBUS_MON_ABUFA_WORD + 1u) * 2u, 0u);
    CHECK(ndbus_servicer_inline_buffer_target(&nd.servicer, MBX_MSG) == 0u,
          "a zero ABUFA resolves to zero");
    CHECK(!ndbus_servicer_write_inline_buffer(&nd.servicer, MBX_MSG,
                                              (const uint8_t *)text, n),
          "and nothing is written, rather than writing over the base of the window");

    ndbus_nd5000_destroy(&nd);
    ndbus_pool_destroy(&pool);
}

static void test_inline_buffer_on_a_mon_stop(void)
{
    printf("Layer 24: a monitor-call stop carries the buffer with it\n");

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the end-to-end stop");
    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);
    NdbusNd5000 nd;
    mbx_init_structures(&pool);
    mbx_attach(&nd, &pool, &fabric);

    nd.servicer.context_area_base = MBX_BASE + 0x3000u;
    s_start_calls = 0;
    s_start_ctx_byte = 0;
    s_start_take = true;
    (void)ndbus_nd5000_set_process_host(&nd, test_start_process);
    (void)ndbus_nd5000_set_data_reader(&nd, test_read_nd500_data_bytes);
    mbx_build_message(&pool, NDBUS_MICFU_START, NDBUS_N5STA_TO_ND500);
    mbx_replay_activation(&pool);
    (void)ndbus_pool_write16(&pool, MBX_EXT1 + NDBUS_MBX_X5ACT_WORD * 2u, 0);
    CHECK(!ndbus_nd5000_service_mailbox(&nd), "a process is started so its message is known");

    const uint32_t target_byte = 0x00003000u;
    const uint32_t target_word = target_byte >> 1;
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MON_ABUFA_WORD * 2u,
                             (uint16_t)(target_word >> 16));
    (void)ndbus_pool_write16(&pool, MBX_MSG + (NDBUS_MON_ABUFA_WORD + 1u) * 2u,
                             (uint16_t)(target_word & 0xFFFFu));

    /* The layout read out of the copy routine at 010662: arg[1]'s VALUE is the
     * count, arg[2]'s ADDRESS is the buffer. Give the two a different shape so a
     * swap cannot pass - arg[1]'s address and arg[2]'s value are decoys. */
    static const char payload[] = "SYSTEM";
    const uint32_t n = 6u;
    for (uint32_t i = 0; i < n; i++) { s_inline_src[i] = (uint8_t)payload[i]; }

    uint32_t addrs[3] = { 0x08000100u, 0x08000200u, 0x1000103Au };
    uint32_t vals[3]  = { 0x00000001u, n,           0x44656365u };

    s_inline_calls = 0;
    s_inline_reader_ok = true;
    CHECK(ndbus_servicer_answer_monitor_call(&nd.servicer, 1u, 0x08005097u,
                                             (uint16_t)NDBUS_MON_504B_NOUTS, 3u, addrs, vals),
          "a MON 504B stop is recorded");
    CHECK(s_inline_calls == 1, "and it asked the host for the buffer exactly once");
    CHECK(s_inline_asked_count == n,
          "the COUNT came from arg[1]'s VALUE, not its address and not arg[2]'s value");
    CHECK(s_inline_asked_addr == addrs[2],
          "and the SOURCE from arg[2]'s ADDRESS, not its value");
    {
        bool ok = true;
        for (uint32_t i = 0; i < n; i++)
        {
            if (ndbus_pool_read8(&pool, target_byte + i) != (uint8_t)payload[i]) { ok = false; }
        }
        CHECK(ok, "the text is in the pool at ABUFA, byte for byte");
    }
    CHECK((ndbus_pool_read16(&pool, MBX_MSG - NDBUS_MON_MIFLAG_BACK_BYTES)
           & NDBUS_MON_MIFLAG_WSMC) != 0u,
          "and WSMC is set, so SINTRAN reads the message rather than asking");

    /* A MON THAT IS NOT IN THE SET MUST NOT ASK AT ALL. Asking is harmless-looking
     * and would mean we had invented an obligation the microcode does not have. */
    s_inline_calls = 0;
    CHECK(ndbus_servicer_answer_monitor_call(&nd.servicer, 1u, 0x08005097u, 0x14Bu, 3u,
                                             addrs, vals),
          "a MON 513B stop is recorded");
    CHECK(s_inline_calls == 0, "and no buffer was fetched for it");

    /* A DECLINING READER LEAVES THE BUFFER ALONE AND STILL RECORDS THE STOP. A
     * refusal here would park the process forever, which is worse than a wrong
     * string. */
    (void)ndbus_pool_write8(&pool, target_byte, 0x5Au);
    s_inline_calls = 0;
    s_inline_reader_ok = false;
    CHECK(ndbus_servicer_answer_monitor_call(&nd.servicer, 1u, 0x08005097u,
                                             (uint16_t)NDBUS_MON_504B_NOUTS, 3u, addrs, vals),
          "a stop whose buffer cannot be read is STILL recorded");
    CHECK(s_inline_calls == 1, "the read was attempted");
    CHECK(ndbus_pool_read8(&pool, target_byte) == 0x5Au,
          "and nothing was written - a partial buffer printed as text is a wrong "
          "answer that looks like an answer");

    ndbus_nd5000_destroy(&nd);
    ndbus_pool_destroy(&pool);
}

static void test_micfu_classes(void)
{
    printf("Layer 21: which MICFU resumes a loaded process and which loads a context\n");

    /* THE WHOLE POINT OF THE SPLIT. 3MONCO and 3TRACO are continues: the microcode's
     * context switch at 011473B finds the wanted process already loaded and skips
     * both the save and the load, so they carry on from the live registers.
     * 3START is NOT a continue even for that same process. */
    CHECK(ndbus_micfu_is_continue(NDBUS_MICFU_MONCO), "3MONCO is a continue");
    CHECK(ndbus_micfu_is_continue(NDBUS_MICFU_TRACO), "3TRACO is a continue");

    /* MEASURED on the octobus macro lane. START-SWAPPER drives fourteen MON 377B
     * rounds, prints "Allocating memory - 7342B pages" and then sends a SECOND
     * 3START for the swapper's own X5CPU 0. Treating it as a continue unparks the
     * process at its MON return address 0x08008255 instead of loading the entry
     * point 0x08000004 that SINTRAN has just written into the context block, and the
     * swapper never re-initialises: the monitor then printed "ADDRESS OUTSIDE
     * PROGRAM SEGMENT / NOT KNOWN TRAP / At program address: 0 1B" with no trap
     * reported from the ND-500 side at all. The reference records the same rule from
     * the microcode: a monitor stop leaves through CALL_MON -> SET_IDLE, which marks
     * "no current process", so the IDLE loop skips CNTXTSAVE and CNTXTLOAD reads
     * SINTRAN's block. */
    CHECK(!ndbus_micfu_is_continue(NDBUS_MICFU_START),
          "3START is NOT a continue - it loads the context block SINTRAN filled");
    CHECK(!ndbus_micfu_is_continue(NDBUS_MICFU_STARTP0),
          "MSG_STARTP0 is not a continue either");

    /* Nothing outside the four is a continue, including the codes that travel on the
     * same message block. */
    CHECK(!ndbus_micfu_is_continue(NDBUS_MICFU_RMICV), "3RMICV is not a continue");
    CHECK(!ndbus_micfu_is_continue(NDBUS_MICFU_SWMESS), "3SWMESS is not a continue");
    CHECK(!ndbus_micfu_is_continue(NDBUS_MICFU_PHYSWR), "PHYSWR is not a continue");
    CHECK(!ndbus_micfu_is_continue(0u), "and neither is zero");

    /* The start CLASS is the wider set - all four reach the host's start hook. The
     * two predicates must not collapse into one another. */
    CHECK(ndbus_micfu_is_start_class(NDBUS_MICFU_STARTP0), "MSG_STARTP0 is start class");
    CHECK(ndbus_micfu_is_start_class(NDBUS_MICFU_START), "3START is start class");
    CHECK(ndbus_micfu_is_start_class(NDBUS_MICFU_MONCO), "3MONCO is start class too");
    CHECK(ndbus_micfu_is_start_class(NDBUS_MICFU_TRACO), "and so is 3TRACO");
    CHECK(!ndbus_micfu_is_start_class(NDBUS_MICFU_RMICV),
          "3RMICV is not - it is answered without touching a process");
    CHECK(!ndbus_micfu_is_start_class(NDBUS_MICFU_CACHE), "nor is MSG_CACHE");
}

static void test_message_x5cpu(void)
{
    printf("Layer 22: X5CPU names the PROCESS, not the station\n");

    NdbusPool pool;
    CHECK(ndbus_pool_create(&pool, 64 * 1024), "a pool for the X5CPU reader");
    NdbusFabric fabric;
    ndbus_fabric_init(&fabric, NULL);
    NdbusNd5000 nd;
    mbx_init_structures(&pool);
    mbx_attach(&nd, &pool, &fabric);

    /* Refusals first, so a -1 can be told from a legitimate process 0. */
    CHECK(ndbus_servicer_read_message_x5cpu(NULL, MBX_MSG) == -1,
          "a NULL servicer is refused");
    CHECK(ndbus_servicer_read_message_x5cpu(&nd.servicer, 0u) == -1,
          "and so is a zero message address");

    /* PROCESS 0 IS THE SWAPPER AND IS A REAL ANSWER. It must not be confused with
     * the refusal above, which is why the reader returns int and not uint16_t. */
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_X5CPU * 2u, 0u);
    CHECK(ndbus_servicer_read_message_x5cpu(&nd.servicer, MBX_MSG) == 0,
          "X5CPU 0 - the swapper - reads as 0, not as a refusal");

    /* PROCESS 1 IS THE FIRST DOMAIN. From PLACE-DOMAIN onward both are live, each
     * with its own message block, and a trap answered on the wrong one is fatal:
     * SINTRAN's TRAPDECODER (MP-P2-N500.NPL:135332) compares the faulting message
     * against the swapper's own and takes EPFINSWAP / XRSTARTALL when they match.
     *
     * MEASURED on the octobus: with the station index used instead of this field,
     * PLACE-DOMAIN CPU-STAT recorded the domain's start as process 0, its page
     * fault went onto the swapper's message 0x8D30 rather than the domain's
     * 0x8E30, and the console printed "*** FATAL SYSTEM ERROR *** / The Swapper
     * stopped" with an otherwise completely correct page-fault record. Reading the
     * field instead put the record on 0x8E30 and SINTRAN answered with a 3MONCO
     * for the swapper - CALL 5ACTSWAPPER, the page-in it is supposed to do. */
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_X5CPU * 2u, 1u);
    CHECK(ndbus_servicer_read_message_x5cpu(&nd.servicer, MBX_MSG) == 1,
          "X5CPU 1 - the first domain - reads as 1");

    /* THE DISTINCTION THIS WHOLE LAYER EXISTS FOR. ndbus_cpu_context_x5cpu() is
     * derived from the STATION NUMBER, so every ND-5000 process on station 070B
     * answers 0 there. The two quantities agree only for process 0, and a test that
     * used process 0 alone would pass with either one. */
    CHECK(ndbus_cpu_context_x5cpu(NDBUS_STATION_ND5000_FIRST) == 0,
          "station 070B is octobus index 0");
    CHECK(ndbus_servicer_read_message_x5cpu(&nd.servicer, MBX_MSG)
              != ndbus_cpu_context_x5cpu(NDBUS_STATION_ND5000_FIRST),
          "and that index is NOT the process number of a domain message");

    /* Every slot the servicer can address, so a wider process number is not
     * silently truncated or sign-extended. */
    (void)ndbus_pool_write16(&pool, MBX_MSG + NDBUS_MSG_X5CPU * 2u, 6u);
    CHECK(ndbus_servicer_read_message_x5cpu(&nd.servicer, MBX_MSG) == 6,
          "the highest ND-5000 process number reads back unchanged");

    ndbus_nd5000_destroy(&nd);
    ndbus_pool_destroy(&pool);
}

static void test_pool_snapshot(void)
{
    printf("Layer 23: a pool snapshot, so a run can be replayed without a boot\n");

    /* WHY THIS EXISTS. Every observation on the octobus lane costs a full
     * SINTRAN boot - about eight minutes to the monitor prompt - and one
     * investigation produced 132 driver scripts for that reason. The mailbox,
     * the message blocks, the segment descriptors and the page tables all live
     * in this pool, so capturing it at the point of interest turns a day of
     * runs into milliseconds. */
    const uint32_t size = 64u * 1024u;
    NdbusPool a, b;
    CHECK(ndbus_pool_create(&a, size), "a pool to snapshot");
    CHECK(ndbus_pool_create(&b, size), "and one to restore into");

    /* Values at the places this lane actually cares about: a message block, a
     * segment-descriptor-shaped word, and the very last byte - the one a
     * short write or an off-by-one length would lose. */
    (void)ndbus_pool_write16(&a, 0x8E30u, 0xBEEFu);
    (void)ndbus_pool_write32(&a, 0x1000u, 0x400000E7u);
    (void)ndbus_pool_write8(&a, size - 1u, 0x5Au);

    char path[512];
    const char *dir = getenv("TMPDIR");
    (void)snprintf(path, sizeof path, "%s/ndbus-pool-snap.bin",
                   (dir != NULL && dir[0] != '\0') ? dir : "/tmp");

    CHECK(ndbus_pool_snapshot_save(&a, path), "the snapshot writes");
    CHECK(ndbus_pool_snapshot_load(&b, path), "and loads into a pool of the same size");

    CHECK(ndbus_pool_read16(&b, 0x8E30u) == 0xBEEFu, "the message block came back");
    CHECK(ndbus_pool_read32(&b, 0x1000u) == 0x400000E7u, "so did the 32-bit entry");
    CHECK(ndbus_pool_read8(&b, size - 1u) == 0x5Au,
          "and the LAST byte, which an off-by-one length would drop");

    /* A SNAPSHOT FROM A DIFFERENT POOL MUST BE REFUSED, NOT READ. Loaded into a
     * pool of another size it would be silently misaligned and every value taken
     * from it afterwards would be confidently wrong - the worst failure shape
     * there is, because it does not look like a failure. */
    NdbusPool wrong;
    CHECK(ndbus_pool_create(&wrong, size * 2u), "a pool of a different size");
    (void)ndbus_pool_write16(&wrong, 0x8E30u, 0x1234u);
    CHECK(!ndbus_pool_snapshot_load(&wrong, path),
          "a snapshot of another size is REFUSED");
    CHECK(ndbus_pool_read16(&wrong, 0x8E30u) == 0x1234u,
          "and the refusal left the pool untouched");

    /* Not a snapshot at all. */
    char junk[512];
    (void)snprintf(junk, sizeof junk, "%s/ndbus-pool-junk.bin",
                   (dir != NULL && dir[0] != '\0') ? dir : "/tmp");
    FILE *jf = fopen(junk, "wb");
    if (jf != NULL)
    {
        (void)fwrite("not a snapshot at all, just some bytes", 1u, 38u, jf);
        (void)fclose(jf);
        CHECK(!ndbus_pool_snapshot_load(&b, junk), "a file with no magic is refused");
        (void)remove(junk);
    }

    CHECK(!ndbus_pool_snapshot_load(&b, "/nonexistent/path/snap.bin"),
          "a missing file is refused rather than leaving a half-loaded pool");

    (void)remove(path);
    ndbus_pool_destroy(&a);
    ndbus_pool_destroy(&b);
    ndbus_pool_destroy(&wrong);
}

int main(void)
{
    printf("MFbus (ndbus) unit tests - no emulator linked\n");
    printf("=============================================\n\n");

    test_pool();
    test_lock_cycle();
    test_doorbell();
    test_window();
    test_octobus();
    test_accp();
    test_context();
    test_nd5000_station();
    test_mailbox();
#ifndef __EMSCRIPTEN__
    test_tset_concurrency();
    test_runners();
    test_exactly_once_canary();
#endif
    test_bringup();
    test_captured_sintran_frames();
    test_test_protocol();
    test_control_store_load();
    test_mailbox_servicer();
    test_accp_guard_matrix();
    test_accp_data_replies();
    test_copy_family_refusals();
    test_doorbell_sniff_rules();
    test_trap_stop_record();
    test_physical_segment_width();
    test_monitor_call_record();
    test_monitor_call_result();
    test_micfu_classes();
    test_stop_kind_selects_the_arm();
    test_inline_user_buffer();
    test_inline_buffer_on_a_mon_stop();
    test_message_x5cpu();
    test_pool_snapshot();

    printf("\n%d check(s), %d failed\n", s_checks, s_failed);
    if (s_failed != 0)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
