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

#include <stdio.h>
#include <string.h>

#include "ndbus_accp.h"
#include "ndbus_doorbell.h"
#include "ndbus_nd5000.h"
#include "ndbus_lock.h"
#include "ndbus_mailbox.h"
#include "ndbus_octobus.h"
#include "ndbus_pool.h"
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
                          &nd120_state};
    NdbusStation nd5000 = {NDBUS_STATION_ND5000_FIRST, "ND-5000 CPU", mock_station_handle,
                           &nd5000_state};

    CHECK(ndbus_fabric_register(&fabric, &nd5000), "station 70B registers");
    CHECK(ndbus_fabric_master(&fabric) == NDBUS_STATION_ND5000_FIRST,
          "the only station is the MASTER");
    CHECK(ndbus_fabric_register(&fabric, &nd120), "station 1B registers");
    CHECK(ndbus_fabric_master(&fabric) == NDBUS_STATION_ND120_CPU,
          "the LOWEST station number becomes MASTER");
    CHECK(ndbus_fabric_station_count(&fabric) == 2, "two stations on the bus");

    /* Station 0 and 77B are not in the T329 table. */
    NdbusStation illegal_low  = {0, "illegal 0", NULL, NULL};
    NdbusStation illegal_high = {63, "illegal 77B", NULL, NULL};
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
    NdbusStation eighth = {63, "an eighth ND-5000", NULL, NULL};
    CHECK(!ndbus_fabric_register(&fabric, &eighth), "there is no eighth slot");

    /* Duplicate registration is refused rather than silently replacing, so two
     * owners fighting over one slot cannot hide. */
    NdbusStation duplicate = {NDBUS_STATION_ND5000_FIRST, "another 70B", NULL, NULL};
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
    NdbusStation silent = {NDBUS_STATION_ND5000_FIRST, "silent", NULL, NULL};
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
static int send_accp(NdbusFabric *fabric, uint8_t from, uint8_t to, const uint8_t *body,
                     int length, uint16_t *replies)
{
    uint16_t dest = (uint16_t)((uint16_t)to << NDBUS_FRAME_STATION_SHIFT);

    uint16_t somb = (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                               NDBUS_FRAME_S_STARTSTOP | (uint16_t)NDBUS_ACCP_OMD);
    (void)ndbus_fabric_send(fabric, from, somb, replies);

    for (int i = 0; i < length; i++)
    {
        uint16_t data = (uint16_t)(dest | (uint16_t)body[i]);
        (void)ndbus_fabric_send(fabric, from, data, replies);
    }

    uint16_t eomb = (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                               (uint16_t)NDBUS_ACCP_OMD);
    return ndbus_fabric_send(fabric, from, eomb, replies);
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
    CHECK(n == 3, "ECHO is answered with Messack plus its two echoed bytes");
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
    CHECK(n == 1 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "LSYSPAR is acked");
    CHECK(nd.accp.system_parameters_given, "and the guard cell is set");

    /* LPARP carries a 4-byte pointer, most significant byte first (T124). */
    body[0] = (uint8_t)NDBUS_ACCP_LPARP;
    body[1] = 0x00;
    body[2] = 0x00;
    body[3] = 0x08;
    body[4] = 0x40;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 5, replies);
    CHECK(n == 1 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "LPARP is acked");
    CHECK(nd.parameter_pointer == 0x00000840u, "the pointer is assembled MSB first");
    CHECK(nd.accp.parameter_pointer_given, "and the guard cell is set");

    /* STARTMIC, then the commands that are illegal while running. */
    body[0] = (uint8_t)NDBUS_ACCP_STARTMIC;
    body[1] = 0;
    body[2] = 0;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 3, replies);
    CHECK(n == 1 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "STARTMIC is acked");
    CHECK(nd.accp.microprogram_running, "the microprogram is running");

    body[0] = (uint8_t)NDBUS_ACCP_VPARP;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    CHECK(n == 1, "VPARP while running is answered");
    CHECK(nd.last_nak_code == NDBUS_ACCP_NAK_MICRO_RUNNING, "with a Messnak of -1");

    /* STOPMIC, and its inverse guard. */
    body[0] = (uint8_t)NDBUS_ACCP_STOPMIC;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    CHECK(n == 1 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "STOPMIC is acked while running");
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
    CHECK(n == 1, "CPURES is answered");
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
    CHECK(n == 2, "the restarted message is answered - Messack plus one echoed byte");
    CHECK((replies[1] & 0xFF) == 0x11,
          "and it echoes the SECOND message's byte, not the abandoned 0x99");
    CHECK(nd.last_command == NDBUS_ACCP_ECHO,
          "and it is the SECOND message, not the abandoned one");

    /* ---- the X5ACT doorbell ------------------------------------------------
     * The signature is the TRANSITION 0xFFFF -> 0. */
    ndbus_nd5000_reset(&nd);
    const uint32_t x5act = 0x700;
    (void)ndbus_pool_write16(&pool, x5act, 0xFFFF);

    CHECK(!ndbus_nd5000_sniff_write16(&nd, x5act, 0xFFFF), "writing -1 over -1 is no transition");
    CHECK(!ndbus_nd5000_sniff_write16(&nd, 0x710, 0), "a cell that was not -1 is not one either");
    CHECK(ndbus_nd5000_sniff_write16(&nd, x5act, 0), "the -1 to 0 write latches the doorbell");
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
    CHECK(!ndbus_nd5000_sniff_write16(&nd, x5act, 0), "the first transition does not latch");
    /* The re-arm writes 1, not -1 - so the next ring is 1 -> 0 and never matches
     * the signature again. This is the whole trap, in one check. */
    (void)ndbus_pool_write16(&pool, x5act, 1);
    CHECK(!ndbus_nd5000_sniff_write16(&nd, x5act, 0),
          "and the re-armed 1 to 0 ring does not match the signature");
    CHECK(!nd.sniff.latched, "so with a threshold of 2 the sniff NEVER latches");

    /* 0 and 1 both mean latch on the first transition. */
    ndbus_nd5000_reset(&nd);
    ndbus_nd5000_set_sniff_threshold(&nd, 0);
    (void)ndbus_pool_write16(&pool, x5act, 0xFFFF);
    CHECK(ndbus_nd5000_sniff_write16(&nd, x5act, 0), "threshold 0 latches on the first");

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
    NdbusStation nd120 = {NDBUS_STATION_ND120_CPU, "ND-120 CPU", NULL, NULL};
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
    CHECK(n == 4, "ECHO answers with Messack plus three bytes");
    CHECK((replies[1] & 0xFF) == 0xDE, "echoing the pattern that was sent");
    CHECK((replies[2] & 0xFF) == 0xAD, "all of it");
    CHECK((replies[3] & 0xFF) == 0xBE, "in order");

    /* 2. LSYSPAR - where the microprogram sends octobus error messages. */
    body[0] = (uint8_t)NDBUS_ACCP_LSYSPAR;
    memset(&body[1], 0, 6);
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 7, replies);
    CHECK(n == 1 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "LSYSPAR is acked");

    /* 3. LPARP - where the parameter area lives in shared memory. */
    const uint32_t param_area = 0x400;
    body[0] = (uint8_t)NDBUS_ACCP_LPARP;
    body[1] = (uint8_t)(param_area >> 24);
    body[2] = (uint8_t)(param_area >> 16);
    body[3] = (uint8_t)(param_area >> 8);
    body[4] = (uint8_t)(param_area & 0xFF);
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 5, replies);
    CHECK(n == 1 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "LPARP is acked");
    CHECK(nd.parameter_pointer == param_area, "and the pointer is where the ND-120 said");

    /* 4. VPARP - THE check that the two agree. The ND-120 writes a word into the
     *    parameter area FIRST, and the ACCP must return THAT word, read out of
     *    shared memory. A canned Messack passes the guard and fails the check
     *    the command exists to perform. */
    (void)ndbus_pool_write32(&pool, param_area, 0xCAFEBABEu);
    body[0] = (uint8_t)NDBUS_ACCP_VPARP;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    CHECK(n == 5, "VPARP answers with Messack plus four bytes");
    uint32_t echoed = ((uint32_t)(replies[1] & 0xFF) << 24) |
                      ((uint32_t)(replies[2] & 0xFF) << 16) |
                      ((uint32_t)(replies[3] & 0xFF) << 8) | (uint32_t)(replies[4] & 0xFF);
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
    echoed = ((uint32_t)(replies[1] & 0xFF) << 24) | ((uint32_t)(replies[2] & 0xFF) << 16) |
             ((uint32_t)(replies[3] & 0xFF) << 8) | (uint32_t)(replies[4] & 0xFF);
    CHECK(echoed == 0x12345678u, "a new parameter pointer reads a different word");

    /* 5. STARTMIC - the microprogram runs. The real start command is 066B; the
     *    manual's section order would have suggested 033B, which is RUNTST. */
    body[0] = (uint8_t)NDBUS_ACCP_STARTMIC;
    body[1] = 0;
    body[2] = 0;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 3, replies);
    CHECK(n == 1 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "STARTMIC is acked");
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
    CHECK(n == 1 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "STOPMIC is acked");

    body[0] = (uint8_t)NDBUS_ACCP_CPURES;
    n = send_accp(&fabric, NDBUS_STATION_ND120_CPU, NDBUS_STATION_ND5000_FIRST, body, 1, replies);
    CHECK(n == 1, "CPURES is acked");
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
    CHECK(n == 1 && nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "and the sequence starts over");

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
    test_nd5000_station();
    test_mailbox();
#ifndef __EMSCRIPTEN__
    test_tset_concurrency();
    test_runners();
#endif
    test_bringup();

    printf("\n%d check(s), %d failed\n", s_checks, s_failed);
    if (s_failed != 0)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
