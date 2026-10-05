/*
 * test_ndbus_station.c - the ND-5000 octobus station against its C# reference:
 *                        kicks, the idle gate, the CPU hooks, CPURES, the
 *                        model/version report, the EOMB rule, the patched
 *                        control-store cells, ECHO, the mailbox configuration
 *                        and ND-500 address zero.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * EVERY TEST HERE PINS ONE DIFFERENCE an audit found between src/ndbus/ndbus_nd5000.c
 * and its reference, $RETROCORE/Emulated.HW/ND/CPU/NDBUS/OctobusND5000Station.cs,
 * on the lane where a functional CPU is attached. Each one names the C# lines it
 * mirrors and is written so that it FAILS on the code as it stood before the port:
 * the comment on each block says what the old code did.
 *
 * Like test_ndbus.c this links nd500_ndbus and nothing else. Frames go in through
 * the fabric, as they do on the machine, and what the station did is read out of
 * shared memory, its reply frames and its own counters.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ndbus_accp.h"
#include "ndbus_mailbox.h"
#include "ndbus_msgqueue.h"
#include "ndbus_nd5000.h"
#include "ndbus_octobus.h"
#include "ndbus_pool.h"
#include "ndbus_servicer.h"
#include "ndbus_testproto.h"

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

#define ST_ND100 ((uint8_t)NDBUS_STATION_ND120_CPU)    /* 1B, the sender in every test */
#define ST_ND5000 ((uint8_t)NDBUS_STATION_ND5000_FIRST) /* 070B */

/* The mailbox fixture. It sits at a non-zero pool offset because start_mess == 0
 * means "no mailbox located" to the station, so a header at offset 0 could not be
 * told from no header at all. */
#define MBX_HEADER     0x4000u            /* X500DF global header, also START_MESS */
#define MBX_EXT1       (MBX_HEADER + 256u) /* CPU 1 extension block */
#define MBX_RING       (MBX_HEADER + 0x800u)
#define MBX_MSG        (MBX_HEADER + 0x1000u)
#define MBX_RING_SLOTS 8u

/* Extension-block BYTE offsets, written out as numbers on purpose: the reference
 * states them as numbers (X5ACT word 5, X5PRO word 6, X5CLR word 0o10, X5CCL word
 * 0o11 - OctobusND5000Station.cs lines 1953-1958) and a test that used the same
 * macros as the code under test could not catch a wrong macro. */
#define EXT_X5ACT_BYTE 0x0Au
#define EXT_X5PRO_BYTE 0x0Cu
#define EXT_X5CLR_BYTE 0x10u
#define EXT_X5CCL_BYTE 0x12u

/* Where the tests put the parameter block LPARP points at. */
#define PARAM_BLOCK 0x0800u

/* ------------------------------------------------------------------------- */
/* helpers                                                                    */
/* ------------------------------------------------------------------------- */

/* The host log: counted, and the last line kept so a test can read what was said. */
static char s_last_log[400];
static int  s_log_lines;

static void capture_log(void *ctx, int level, const char *message)
{
    (void)ctx;
    (void)level;
    (void)snprintf(s_last_log, sizeof(s_last_log), "%s", message);
    s_log_lines++;
}

/* A station at 1B that only records what the bus delivers to it directly. */
typedef struct Capture
{
    uint16_t frames[64];
    int      count;
} Capture;

static int capture_handle(NdbusStation *station, uint16_t frame, uint8_t source_station,
                          uint16_t *replies)
{
    Capture *capture = (Capture *)station->ctx;
    (void)source_station;
    (void)replies;

    if (capture->count < (int)(sizeof(capture->frames) / sizeof(capture->frames[0])))
    {
        capture->frames[capture->count] = frame;
    }
    capture->count++;
    return 0;
}

/* Everything one test needs: a pool, a fabric, the ND-5000 station at `station`
 * and a recording station at 1B. */
typedef struct Rig
{
    NdbusPool    pool;
    NdbusFabric  fabric;
    NdbusHostOps host;
    NdbusNd5000  nd;
    NdbusStation nd100;
    Capture      capture;
} Rig;

static void rig_up(Rig *rig, uint32_t pool_bytes, uint8_t station)
{
    memset(rig, 0, sizeof(*rig));
    CHECK(ndbus_pool_create(&rig->pool, pool_bytes), "rig: the pool is created");

    rig->host.log = capture_log;
    ndbus_fabric_init(&rig->fabric, NULL);

    CHECK(ndbus_nd5000_init(&rig->nd, station, &rig->pool, &rig->host, NULL),
          "rig: the ND-5000 station comes up");
    CHECK(ndbus_fabric_register(&rig->fabric, &rig->nd.station), "rig: and registers");

    rig->nd100.number = ST_ND100;
    rig->nd100.type   = "ND-100 capture";
    rig->nd100.handle = capture_handle;
    rig->nd100.ctx    = &rig->capture;
    CHECK(ndbus_fabric_register(&rig->fabric, &rig->nd100), "rig: the ND-100 joins at 1B");

    s_log_lines   = 0;
    s_last_log[0] = '\0';
}

static void rig_down(Rig *rig)
{
    ndbus_nd5000_destroy(&rig->nd);
    ndbus_pool_destroy(&rig->pool);
}

/* One multibyte message to OMD `omd`: SOMB, source OMD, byte count, the payload,
 * EOMB - the whole envelope, because the station reads the command at byte 2. */
static int send_message(NdbusFabric *fabric, uint8_t to, uint8_t omd, const uint8_t *payload,
                        int length, uint16_t *replies)
{
    uint16_t dest = (uint16_t)((uint16_t)to << NDBUS_FRAME_STATION_SHIFT);

    uint16_t somb = (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                               NDBUS_FRAME_S_STARTSTOP | (uint16_t)omd);
    (void)ndbus_fabric_send(fabric, ST_ND100, somb, replies);
    (void)ndbus_fabric_send(fabric, ST_ND100, (uint16_t)(dest | (uint16_t)NDBUS_ACCP_OMD), replies);
    (void)ndbus_fabric_send(fabric, ST_ND100, (uint16_t)(dest | (uint16_t)length), replies);
    for (int i = 0; i < length; i++)
    {
        (void)ndbus_fabric_send(fabric, ST_ND100, (uint16_t)(dest | (uint16_t)payload[i]), replies);
    }

    uint16_t eomb = (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                               (uint16_t)omd);
    return ndbus_fabric_send(fabric, ST_ND100, eomb, replies);
}

static int send_accp(Rig *rig, const uint8_t *payload, int length, uint16_t *replies)
{
    return send_message(&rig->fabric, rig->nd.station.number, (uint8_t)NDBUS_ACCP_OMD, payload,
                        length, replies);
}

/* One ACCP command with no parameters. */
static int send_command(Rig *rig, uint8_t command, uint16_t *replies)
{
    return send_accp(rig, &command, 1, replies);
}

/* One ACCP command followed by `count` zero parameter bytes. */
static int send_command_zeros(Rig *rig, uint8_t command, int count, uint16_t *replies)
{
    uint8_t body[1 + 8];
    memset(body, 0, sizeof(body));
    body[0] = command;
    return send_accp(rig, body, 1 + count, replies);
}

static int send_frame(Rig *rig, uint16_t frame, uint16_t *replies)
{
    return ndbus_fabric_send(&rig->fabric, ST_ND100, frame, replies);
}

/* A kick: C=1, the destination, K=1 and the kick number in bits 5-0. Kick 1 to
 * station 1 is 100501B, the word the reference quotes from the microcode. */
static uint16_t kick_frame(uint8_t to, unsigned number)
{
    return (uint16_t)(NDBUS_FRAME_C_CONTROL | ((uint16_t)to << NDBUS_FRAME_STATION_SHIFT) |
                      NDBUS_FRAME_K_KICK | (uint16_t)(number & 0x3Fu));
}

/* An emergency: C=1, the destination and the WHOLE information byte (241B, 242B,
 * 244B all have the E bit, bit 7, set). */
static uint16_t emergency_frame(uint8_t to, uint8_t code)
{
    return (uint16_t)(NDBUS_FRAME_C_CONTROL | ((uint16_t)to << NDBUS_FRAME_STATION_SHIFT) |
                      (uint16_t)code);
}

/* One multibyte message read back out of a run of reply frames. */
typedef struct Message
{
    uint8_t dest_omd;
    uint8_t source_omd;
    uint8_t station; /* bits 13-8 of its frames: who sent it */
    int     length;
    uint8_t payload[64];
} Message;

/*
 * Unwrap the message that starts at frames[*index] and advance *index past it.
 * Checks the whole envelope: SOMB with M and S, EOMB with M and no S, the same OMD
 * in both, the declared count equal to what is there, and one station number in
 * every frame. Returns false, with the reason printed, when any of that is wrong.
 */
static bool next_message(const uint16_t *frames, int n, int *index, Message *out)
{
    int i = *index;
    memset(out, 0, sizeof(*out));

    if ((n - i) < 4)
    {
        printf("  %d frame(s) left at %d: too few for an envelope\n", n - i, i);
        return false;
    }

    uint16_t somb = frames[i];
    if ((somb & NDBUS_FRAME_C_CONTROL) == 0 || (somb & NDBUS_FRAME_M_MULTIBYTE) == 0 ||
        (somb & NDBUS_FRAME_S_STARTSTOP) == 0)
    {
        printf("  frame %d (%06o) is not a SOMB\n", i, (unsigned)somb);
        return false;
    }

    out->dest_omd   = (uint8_t)(somb & NDBUS_FRAME_CODE_MASK);
    out->station    = ndbus_frame_station(somb);
    out->source_omd = (uint8_t)(frames[i + 1] & NDBUS_FRAME_DATA_MASK);
    out->length     = (int)(frames[i + 2] & NDBUS_FRAME_DATA_MASK);

    if (out->length > (int)sizeof(out->payload) || (n - i) < (4 + out->length))
    {
        printf("  message at %d declares %d byte(s), which does not fit\n", i, out->length);
        return false;
    }
    for (int k = 0; k < out->length; k++)
    {
        out->payload[k] = (uint8_t)(frames[i + 3 + k] & NDBUS_FRAME_DATA_MASK);
    }

    uint16_t eomb = frames[i + 3 + out->length];
    if ((eomb & NDBUS_FRAME_C_CONTROL) == 0 || (eomb & NDBUS_FRAME_M_MULTIBYTE) == 0 ||
        (eomb & NDBUS_FRAME_S_STARTSTOP) != 0 ||
        (uint8_t)(eomb & NDBUS_FRAME_CODE_MASK) != out->dest_omd)
    {
        printf("  frame %d (%06o) is not the EOMB of OMD %u\n", i + 3 + out->length,
               (unsigned)eomb, (unsigned)out->dest_omd);
        return false;
    }

    for (int k = 0; k < (4 + out->length); k++)
    {
        if (ndbus_frame_station(frames[i + k]) != out->station)
        {
            printf("  frame %d carries another station number than the SOMB\n", i + k);
            return false;
        }
    }

    *index = i + 4 + out->length;
    return true;
}

/* LPARP: the 4-byte parameter pointer, most significant byte first. */
static void send_lparp(Rig *rig, uint32_t pointer)
{
    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
    uint8_t  body[5];
    body[0] = (uint8_t)NDBUS_ACCP_LPARP;
    body[1] = (uint8_t)(pointer >> 24u);
    body[2] = (uint8_t)(pointer >> 16u);
    body[3] = (uint8_t)(pointer >> 8u);
    body[4] = (uint8_t)(pointer & 0xFFu);
    (void)send_accp(rig, body, 5, replies);
}

/* LSYSPAR with word 1 = `word1` and two zero words. 0x0108 is the live value:
 * N100IDENT 1 in the high byte, 5OMDNO 10B in the low byte. */
static void send_lsyspar(Rig *rig, uint16_t word1)
{
    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
    uint8_t  body[7];
    memset(body, 0, sizeof(body));
    body[0] = (uint8_t)NDBUS_ACCP_LSYSPAR;
    body[1] = (uint8_t)(word1 >> 8u);
    body[2] = (uint8_t)(word1 & 0xFFu);
    (void)send_accp(rig, body, 7, replies);
}

/*
 * Get the station a control store the way the machine does: LPARP, then one LOCSM
 * pulse of one all-zero microword at control-store address 0. Individual cells are
 * then set with cs_poke*, which is what the existing control-store tests do too -
 * the LOAD has its own tests; these tests are about what is READ from the store.
 */
static void cs_allocate(Rig *rig)
{
    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];

    send_lparp(rig, PARAM_BLOCK);
    (void)ndbus_pool_write16(&rig->pool, PARAM_BLOCK, 1u);      /* N = 1 microword */
    (void)ndbus_pool_write16(&rig->pool, PARAM_BLOCK + 2u, 0u); /* at address 0 */
    for (uint32_t i = 0u; i < 8u; i++)
    {
        (void)ndbus_pool_write16(&rig->pool, PARAM_BLOCK + 4u + (i * 2u), 0u);
    }
    (void)send_command(rig, (uint8_t)NDBUS_ACCP_LOCSM, replies);
    CHECK(rig->nd.control_store != NULL, "rig: the control store is allocated");
}

static void cs_poke(Rig *rig, uint32_t cs_word, uint32_t halfword, uint16_t value)
{
    if (rig->nd.control_store != NULL)
    {
        rig->nd.control_store[(cs_word * NDBUS_CS_HALFWORDS_PER_WORD) + halfword] = value;
    }
}

/* A LARG constant: the 32-bit value in halfwords 6 and 7, high half first. */
static void cs_poke_larg(Rig *rig, uint32_t cs_word, uint32_t value)
{
    cs_poke(rig, cs_word, 6u, (uint16_t)(value >> 16u));
    cs_poke(rig, cs_word, 7u, (uint16_t)(value & 0xFFFFu));
}

/* XMSINIT's picture of the mailbox: the global header and CPU 1's extension block. */
static void mbx_init_structures(NdbusPool *pool)
{
    (void)ndbus_pool_write16(pool, MBX_HEADER + NDBUS_MBX_X5SEM_WORD * 2u, 0u);
    (void)ndbus_pool_write16(pool, MBX_HEADER + NDBUS_MBX_X5HEN_WORD * 2u, 0u);
    (void)ndbus_pool_write16(pool, MBX_HEADER + NDBUS_MBX_X5FYL_WORD * 2u, 0u);
    (void)ndbus_pool_write16(pool, MBX_HEADER + NDBUS_MBX_X5MXF_WORD * 2u, MBX_RING_SLOTS);
    (void)ndbus_pool_write16(pool, MBX_HEADER + NDBUS_MBX_X5FIF_WORD * 2u,
                             (uint16_t)(MBX_RING >> 16u));
    (void)ndbus_pool_write16(pool, MBX_HEADER + (NDBUS_MBX_X5FIF_WORD + 1u) * 2u,
                             (uint16_t)(MBX_RING & 0xFFFFu));

    (void)ndbus_pool_write16(pool, MBX_EXT1 + 0u, 0xFFFFu); /* X5BEX = -1, empty chain */
    (void)ndbus_pool_write16(pool, MBX_EXT1 + 2u, 0xFFFFu);
    (void)ndbus_pool_write16(pool, MBX_EXT1 + EXT_X5ACT_BYTE, 0xFFFFu);
    (void)ndbus_pool_write16(pool, MBX_EXT1 + EXT_X5PRO_BYTE, 0xFFFFu);
}

/* One message block for the ND-500, LINK = -1, queued at the head of the chain.
 * The X5ACT doorbell is deliberately NOT rung: these tests are about kicks. */
static void mbx_queue_message(NdbusPool *pool, uint16_t micfu)
{
    (void)ndbus_pool_write16(pool, MBX_MSG + 0u, 0xFFFFu);
    (void)ndbus_pool_write16(pool, MBX_MSG + 2u, 0xFFFFu);
    (void)ndbus_pool_write16(pool, MBX_MSG + NDBUS_MSG_N5STA * 2u, NDBUS_N5STA_TO_ND500);
    (void)ndbus_pool_write16(pool, MBX_MSG + NDBUS_MSG_X5CPU * 2u, 1u);
    (void)ndbus_pool_write16(pool, MBX_MSG + NDBUS_MSG_MICFU * 2u, micfu);

    (void)ndbus_pool_write16(pool, MBX_EXT1 + 0u, (uint16_t)(MBX_MSG >> 16u));
    (void)ndbus_pool_write16(pool, MBX_EXT1 + 2u, (uint16_t)(MBX_MSG & 0xFFFFu));
}

static uint16_t mbx_message_status(const NdbusPool *pool)
{
    return (uint16_t)(ndbus_pool_read16(pool, MBX_MSG + NDBUS_MSG_N5STA * 2u) & NDBUS_N5STA_MASK);
}

/*
 * A station with its mailbox located the way the machine locates it: a control
 * store whose START_MESS cell (026B) holds the header offset and whose SAMSON_CPU
 * cell (025B) holds 1, read at ENKICK. Kicks are enabled by that same ENKICK.
 */
static void rig_up_with_mailbox(Rig *rig)
{
    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];

    rig_up(rig, 64u * 1024u, ST_ND5000);
    mbx_init_structures(&rig->pool);
    cs_allocate(rig);
    cs_poke_larg(rig, 0x16u, MBX_HEADER); /* 026B START_MESS */
    cs_poke_larg(rig, 0x15u, 1u);         /* 025B SAMSON_CPU */
    (void)send_command(rig, (uint8_t)NDBUS_ACCP_ENKICK, replies);

    CHECK(rig->nd.start_mess == MBX_HEADER, "rig: ENKICK located the mailbox header");
    CHECK(ndbus_mailbox_ext_base(&rig->nd.mailbox) == MBX_EXT1,
          "rig: and CPU 1's extension block");
    CHECK(rig->nd.accp.kicks_enabled, "rig: kicks are enabled");
}

/* ------------------------------------------------------------------------- */
/* 1. Kicks                                                                   */
/* ------------------------------------------------------------------------- */

/*
 * OctobusND5000Station.cs HandleFrame, lines 2399-2523, with WalkQueue
 * (2072-2128), ExecuteClearFunctions (1994-2022) and GoIdle (2050-2064).
 *
 * BEFORE THE PORT every kick fell into "accepted and silent": nothing was counted,
 * no queue was walked, X5CLR was never acknowledged and X5PRO never went idle.
 */
static void test_kicks(void)
{
    printf("1. Kicks\n");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];

    /* ---- the gate, the tally and the broadcast rule, with no mailbox at all -- */
    {
        Rig rig;
        rig_up(&rig, 4096u, ST_ND5000);

        int n = send_frame(&rig, kick_frame(ST_ND5000, 1u), replies);
        CHECK(n == 0, "a kick has no reply frame");
        CHECK(rig.nd.kick_counts[1] == 1u, "a kick is counted even while kicks are disabled");
        CHECK(rig.nd.kicks_dropped_disabled == 1u, "and counted as dropped because of that");

        (void)send_frame(&rig, kick_frame(ST_ND5000, 31u), replies);
        CHECK(rig.nd.kick_counts[31] == 1u, "kick 31 is counted in its own slot");
        CHECK(rig.nd.kicks_dropped_disabled == 2u, "and it is dropped too");

        /* A frame with K AND M set is multibyte traffic, not a kick: the reference
         * tests "isControl && isKick && !isMultibyte" (line 2399). Bit 5 is both
         * the M flag and the top bit of the kick number, so "kick 35" is in fact
         * an EOMB for OMD 3, and with no message open it does nothing. */
        n = send_frame(&rig, kick_frame(ST_ND5000, 35u), replies);
        CHECK(n == 0 && rig.nd.kick_counts[35] == 0u && rig.nd.kick_counts[3] == 0u,
              "a control frame with K and M both set is not decoded as a kick");
        CHECK(rig.nd.kicks_dropped_disabled == 2u, "and is not counted as a dropped kick");

        /* Broadcast is not allowed for kicks, and a rejected one is not counted. */
        n = send_frame(&rig, (uint16_t)(kick_frame(ST_ND5000, 3u) | NDBUS_FRAME_B_BROADCAST),
                       replies);
        CHECK(n == 0, "a broadcast kick has no reply either");
        CHECK(rig.nd.kick_counts[3] == 0u, "a broadcast kick is rejected before it is counted");
        CHECK(rig.nd.kicks_dropped_disabled == 2u, "and is not a 'dropped, disabled' kick");

        /* With kicks enabled but no mailbox, kick 1 has nothing to walk and kicks
         * 3 and 6 say so instead of writing through an unconfigured block. */
        (void)send_command(&rig, (uint8_t)NDBUS_ACCP_ENKICK, replies);
        CHECK(rig.nd.accp.kicks_enabled, "ENKICK enables kicks");

        (void)send_frame(&rig, kick_frame(ST_ND5000, 1u), replies);
        CHECK(rig.nd.kick_counts[1] == 2u, "an enabled kick 1 is counted");
        CHECK(rig.nd.kicks_dropped_disabled == 2u, "and no longer counted as dropped");

        s_log_lines = 0;
        (void)send_frame(&rig, kick_frame(ST_ND5000, 3u), replies);
        CHECK(s_log_lines == 1 && strstr(s_last_log, "KICK 3") != NULL &&
                  strstr(s_last_log, "ignored") != NULL,
              "kick 3 with no mailbox logs that it was ignored");
        s_log_lines = 0;
        (void)send_frame(&rig, kick_frame(ST_ND5000, 6u), replies);
        CHECK(s_log_lines == 1 && strstr(s_last_log, "KICK 6") != NULL &&
                  strstr(s_last_log, "ignored") != NULL,
              "kick 6 with no mailbox logs that it was ignored");
        CHECK(ndbus_pool_read16(&rig.pool, EXT_X5PRO_BYTE) == 0u &&
                  ndbus_pool_read16(&rig.pool, 256u + EXT_X5PRO_BYTE) == 0u,
              "and neither wrote into the pool");

        rig_down(&rig);
    }

    /* ---- kick 1 and kick 2 walk the queue WITHOUT touching X5ACT ------------- */
    for (unsigned number = 1u; number <= 2u; number++)
    {
        Rig rig;
        rig_up_with_mailbox(&rig);
        send_lsyspar(&rig, 0x0108u); /* so the answer interrupt has a destination */

        mbx_queue_message(&rig.pool, NDBUS_MICFU_RMICV);
        CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5ACT_BYTE) == 0xFFFFu,
              "the doorbell is NOT rung: X5ACT is still -1");

        rig.capture.count = 0;
        int n = send_frame(&rig, kick_frame(ST_ND5000, number), replies);
        CHECK(n == 0, "the kick itself has no reply frame");
        CHECK(rig.nd.kick_counts[number] == 1u, "the kick is counted");
        CHECK(mbx_message_status(&rig.pool) == NDBUS_N5STA_ANSWER,
              "the queued message is answered by the kick alone");
        CHECK(ndbus_pool_read16(&rig.pool, MBX_MSG + NDBUS_MSG_N500A * 2u) ==
                  NDBUS_SERVICER_MICRO_VERSION_DEFAULT,
              "with the 3RMICV answer written into it");
        CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5ACT_BYTE) == 0xFFFFu,
              "and X5ACT was not touched - the kick enters the walk directly");
        CHECK(rig.nd.service_polls == 0u && rig.nd.service_activations == 0u,
              "the X5ACT poll did not run at all");
        CHECK(rig.capture.count == 1,
              "the answer interrupt frame reached the ND-100 at station 1");

        rig_down(&rig);
    }

    /* ---- kick 3, CLRKICK: X5CCL := 1, X5PRO := -1, X5CLR := 0 ---------------- */
    {
        Rig rig;
        rig_up_with_mailbox(&rig);

        (void)ndbus_pool_write16(&rig.pool, MBX_EXT1 + EXT_X5CLR_BYTE, 0077u); /* ST0PSYS's mask */
        (void)ndbus_pool_write16(&rig.pool, MBX_EXT1 + EXT_X5CCL_BYTE, 0u);
        (void)ndbus_pool_write16(&rig.pool, MBX_EXT1 + EXT_X5PRO_BYTE, 5u);    /* a process */

        s_log_lines = 0;
        int n = send_frame(&rig, kick_frame(ST_ND5000, 3u), replies);
        CHECK(n == 0, "CLRKICK has no reply frame");
        CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5CLR_BYTE) == 0u,
              "X5CLR (ext + 0x10) is acknowledged with a plain zero - what ST0PSYS polls for");
        CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5CCL_BYTE) == 1u,
              "X5CCL (ext + 0x12) is set to 1");
        CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5PRO_BYTE) == 0xFFFFu,
              "X5PRO (ext + 0x0C) is set to -1, idle");
        CHECK(s_log_lines == 1 && strstr(s_last_log, "mask=0x003F") != NULL,
              "and the log line carries the mask that was read, 0o77");
        CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5ACT_BYTE) == 0xFFFFu,
              "X5ACT is not part of the clear functions");

        rig_down(&rig);
    }

    /* ---- kick 6, IDLEKICK: X5PRO := -1 and nothing else ---------------------- */
    {
        Rig rig;
        rig_up_with_mailbox(&rig);

        (void)ndbus_pool_write16(&rig.pool, MBX_EXT1 + EXT_X5PRO_BYTE, 5u);
        (void)ndbus_pool_write16(&rig.pool, MBX_EXT1 + EXT_X5CLR_BYTE, 0077u);
        (void)ndbus_pool_write16(&rig.pool, MBX_EXT1 + EXT_X5CCL_BYTE, 0u);

        (void)send_frame(&rig, kick_frame(ST_ND5000, 6u), replies);
        CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5PRO_BYTE) == 0xFFFFu,
              "IDLEKICK sets X5PRO to -1 - what TER51 polls for");
        CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5CLR_BYTE) == 0077u,
              "and leaves X5CLR alone");
        CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5CCL_BYTE) == 0u,
              "and X5CCL alone");

        rig_down(&rig);
    }

    /* ---- every other kick number: logged as not implemented, nothing written -- */
    {
        Rig rig;
        rig_up_with_mailbox(&rig);

        (void)ndbus_pool_write16(&rig.pool, MBX_EXT1 + EXT_X5PRO_BYTE, 5u);
        (void)ndbus_pool_write16(&rig.pool, MBX_EXT1 + EXT_X5CLR_BYTE, 0077u);

        static const unsigned unknown[] = { 0u, 4u, 5u, 7u, 31u };
        for (size_t i = 0; i < sizeof(unknown) / sizeof(unknown[0]); i++)
        {
            s_log_lines = 0;
            (void)send_frame(&rig, kick_frame(ST_ND5000, unknown[i]), replies);
            CHECK(rig.nd.kick_counts[unknown[i]] == 1u, "an unimplemented kick is still counted");
            CHECK(s_log_lines == 1 && strstr(s_last_log, "NOT IMPLEMENTED") != NULL,
                  "and is logged as NOT IMPLEMENTED, unconditionally");

            bool kick05 = (unknown[i] == 4u || unknown[i] == 5u);
            CHECK(strstr(s_last_log, kick05 ? "OCB_KICK05" : "NOTREC 204") != NULL,
                  "naming the microcode routine that number would have run");
        }
        CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5PRO_BYTE) == 5u &&
                  ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5CLR_BYTE) == 0077u,
              "none of them wrote into the extension block");

        rig_down(&rig);
    }
}

/* ------------------------------------------------------------------------- */
/* 2. The idle gate                                                           */
/* ------------------------------------------------------------------------- */

/*
 * OctobusND5000Station.cs HandleFrame, lines 2383-2397, and the three places the
 * flag is cleared: 242B (ContinueAccp, line 4300), 241B (ResetStation, line 4318)
 * and the 066B/035B/036B arm (line 3392).
 *
 * BEFORE THE PORT accp_idle was written and never read, so a terminated ACCP went
 * on taking every frame.
 */
static void test_idle_gate(void)
{
    printf("2. The idle gate\n");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];

    Rig rig;
    rig_up_with_mailbox(&rig);
    (void)ndbus_pool_write16(&rig.pool, MBX_EXT1 + EXT_X5CLR_BYTE, 0077u);

    /* 244B TERMINATE ACCP: into the idle loop. Kicks stay enabled - the gate, not
     * the kicks-enabled cell, is what must stop the kick below. */
    (void)send_frame(&rig, emergency_frame(ST_ND5000, NDBUS_EMERGENCY_TERMINATE_ACCP), replies);
    CHECK(rig.nd.accp_idle, "244B puts the ACCP in its idle loop");
    CHECK(rig.nd.accp.kicks_enabled, "and does not touch the kicks-enabled cell");

    (void)send_frame(&rig, kick_frame(ST_ND5000, 3u), replies);
    CHECK(rig.nd.kick_counts[3] == 0u, "a kick that arrives while idle never reaches the decoder");
    CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5CLR_BYTE) == 0077u,
          "so X5CLR is not acknowledged");

    /* A message for another OMD is dropped whole while idle: its SOMB, its data
     * and its EOMB. Nothing is collected. */
    static const uint8_t other[1] = { 0x55u };
    unsigned long        handled  = rig.nd.messages_handled;
    int                  n = send_message(&rig.fabric, ST_ND5000, 5u, other, 1, replies);
    CHECK(n == 0 && !rig.nd.inbox.open && rig.nd.messages_handled == handled,
          "an OMD-5 message is dropped while idle");

    /* OMD 3 still gets through, frame by frame: the SOMB opens the message, the
     * data bytes are let in because a message is open, the EOMB runs it. ALIVE is
     * the command SINTRAN sends right after a terminate; the microprogram is
     * stopped, so the answer is Messnak 7 - and it IS an answer. */
    n = send_command(&rig, (uint8_t)NDBUS_ACCP_ALIVE, replies);
    Message       reply;
    int           index = 0;
    CHECK(n > 0 && next_message(replies, n, &index, &reply),
          "an OMD-3 command is still answered while idle");
    CHECK(rig.nd.messages_handled == handled + 1u, "the ACCP ran it");
    CHECK(reply.length == 4 && reply.payload[0] == 0xFFu &&
              reply.payload[1] == (uint8_t)NDBUS_ACCP_NAK_NOT_ALIVE,
          "ALIVE after a terminate answers Messnak 7, not alive");
    CHECK(rig.nd.accp_idle, "answering a command does not leave the idle loop");

    /* OMD 0, the Test Protocol, is the other OMD the gate lets through. */
    uint16_t somb0 = (uint16_t)(NDBUS_FRAME_C_CONTROL |
                                ((uint16_t)ST_ND5000 << NDBUS_FRAME_STATION_SHIFT) |
                                NDBUS_FRAME_M_MULTIBYTE | NDBUS_FRAME_S_STARTSTOP |
                                (uint16_t)NDBUS_TESTPROTO_OMD);
    (void)send_frame(&rig, somb0, replies);
    CHECK(rig.nd.inbox.open, "an OMD-0 SOMB opens a message while idle");
    (void)send_frame(&rig, (uint16_t)(somb0 & (uint16_t)~NDBUS_FRAME_S_STARTSTOP), replies);
    CHECK(!rig.nd.inbox.open, "and its EOMB closes it");

    /* 035B CONTMIC starts the microprogram and with it leaves the idle loop; the
     * same kick now goes through. */
    (void)send_command(&rig, (uint8_t)NDBUS_ACCP_CONTMIC, replies);
    CHECK(!rig.nd.accp_idle, "035B CONTMIC clears the idle flag");
    (void)send_frame(&rig, kick_frame(ST_ND5000, 3u), replies);
    CHECK(rig.nd.kick_counts[3] == 1u, "after the start the kick is decoded");
    CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5CLR_BYTE) == 0u,
          "and X5CLR is acknowledged");

    /* 242B CONTINUE ACCP clears it too. */
    (void)send_frame(&rig, emergency_frame(ST_ND5000, NDBUS_EMERGENCY_TERMINATE_ACCP), replies);
    (void)ndbus_pool_write16(&rig.pool, MBX_EXT1 + EXT_X5PRO_BYTE, 5u);
    (void)send_frame(&rig, kick_frame(ST_ND5000, 6u), replies);
    CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5PRO_BYTE) == 5u,
          "idle again: kick 6 is dropped");
    (void)send_frame(&rig, emergency_frame(ST_ND5000, NDBUS_EMERGENCY_CONTINUE_ACCP), replies);
    CHECK(!rig.nd.accp_idle, "242B CONTINUE ACCP clears the idle flag");
    (void)send_frame(&rig, kick_frame(ST_ND5000, 6u), replies);
    CHECK(ndbus_pool_read16(&rig.pool, MBX_EXT1 + EXT_X5PRO_BYTE) == 0xFFFFu,
          "and kick 6 is carried out");

    /* 066B STARTMIC and 036B RESTMIC clear it as well. STOPMIC goes between them
     * because the guard table refuses either start while the microprogram runs. */
    (void)send_frame(&rig, emergency_frame(ST_ND5000, NDBUS_EMERGENCY_TERMINATE_ACCP), replies);
    CHECK(rig.nd.accp_idle, "idle for the 066B case");
    (void)send_command_zeros(&rig, (uint8_t)NDBUS_ACCP_STARTMIC, 2, replies);
    CHECK(!rig.nd.accp_idle, "066B STARTMIC clears the idle flag");

    (void)send_frame(&rig, emergency_frame(ST_ND5000, NDBUS_EMERGENCY_TERMINATE_ACCP), replies);
    CHECK(rig.nd.accp_idle && !rig.nd.accp.microprogram_running, "idle for the 036B case");
    (void)send_command_zeros(&rig, (uint8_t)NDBUS_ACCP_RESTMIC, 4, replies);
    CHECK(!rig.nd.accp_idle, "036B RESTMIC clears the idle flag");

    rig_down(&rig);
}

/* ------------------------------------------------------------------------- */
/* 3. The CPU hooks, and 4. what CPURES clears                                */
/* ------------------------------------------------------------------------- */

/* What the two callbacks saw, so the ORDER of each call can be asserted. */
typedef struct HookLog
{
    const NdbusNd5000 *nd;
    int                resets;
    int                inits;
    bool               running_at_reset;
    bool               kicks_at_reset;
    bool               syspar_at_reset;
    bool               running_at_init;
    uint32_t           context_area_at_init;
} HookLog;

static void hook_reset_cpu_to_idle(void *ctx)
{
    HookLog *log = (HookLog *)ctx;
    log->resets++;
    log->running_at_reset = log->nd->accp.microprogram_running;
    log->kicks_at_reset   = log->nd->accp.kicks_enabled;
    log->syspar_at_reset  = log->nd->accp.system_parameters_given;
}

static void hook_apply_init_state(void *ctx)
{
    HookLog *log = (HookLog *)ctx;
    log->inits++;
    log->running_at_init      = log->nd->accp.microprogram_running;
    log->context_area_at_init = log->nd->context_area;
}

/*
 * OctobusND5000Station.cs: ResetCpuToIdle (2652-2663) called from the 241B arm
 * (2674-2675) and the 071B arm (3157); ApplyND5000InitState called from the 066B
 * arm only (3447-3448). CPURES state: lines 3151-3168.
 *
 * BEFORE THE PORT the station had no way to reach a CPU at all, and CPURES ran the
 * full cold reset: it forgot the system parameters and the parameter pointer and
 * re-armed the doorbell sniff.
 */
static void test_cpu_hooks_and_cpures(void)
{
    printf("3. The CPU hooks, and 4. what CPURES clears\n");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];

    CHECK(!ndbus_nd5000_set_cpu_hooks(NULL, hook_reset_cpu_to_idle, hook_apply_init_state, NULL),
          "installing hooks on no station is refused");

    Rig     rig;
    HookLog log;
    rig_up(&rig, 64u * 1024u, ST_ND5000);
    memset(&log, 0, sizeof(log));
    log.nd = &rig.nd;
    CHECK(ndbus_nd5000_set_cpu_hooks(&rig.nd, hook_reset_cpu_to_idle, hook_apply_init_state, &log),
          "the two hooks are installed");

    /* A patched context-area cell, so the 066B hook can show the cells were read
     * BEFORE it was called. */
    cs_allocate(&rig);
    cs_poke_larg(&rig, 0x10u, 0x0002A000u);

    send_lsyspar(&rig, 0x0108u);
    CHECK(rig.nd.accp.system_parameters_given && rig.nd.accp.parameter_pointer_given,
          "LSYSPAR and LPARP have been given");

    /* ---- 066B: apply_init_state, once, after the cells were read ---------- */
    int n = send_command_zeros(&rig, (uint8_t)NDBUS_ACCP_STARTMIC, 2, replies);
    CHECK(n == 5, "066B STARTMIC is acknowledged");
    CHECK(log.inits == 1, "066B calls apply_init_state exactly once");
    CHECK(log.resets == 0, "and does not reset the CPU");
    CHECK(log.running_at_init, "the microprogram was already marked running at the call");
    CHECK(log.context_area_at_init == 0x0002A000u,
          "and the patched cells had already been read");

    /* ---- 035B and 036B resume; they do not run INIT again ------------------ */
    (void)send_command(&rig, (uint8_t)NDBUS_ACCP_STOPMIC, replies);
    (void)send_command(&rig, (uint8_t)NDBUS_ACCP_CONTMIC, replies);
    CHECK(rig.nd.accp.microprogram_running, "035B CONTMIC started the microprogram");
    CHECK(log.inits == 1, "035B CONTMIC does not call apply_init_state");
    (void)send_command(&rig, (uint8_t)NDBUS_ACCP_STOPMIC, replies);
    (void)send_command_zeros(&rig, (uint8_t)NDBUS_ACCP_RESTMIC, 4, replies);
    CHECK(rig.nd.accp.microprogram_running, "036B RESTMIC started the microprogram");
    CHECK(log.inits == 1, "036B RESTMIC does not call apply_init_state");

    /* ---- 071B CPURES: reset the CPU, clear TWO cells, keep the rest -------- */
    (void)send_command(&rig, (uint8_t)NDBUS_ACCP_ENKICK, replies);
    CHECK(rig.nd.accp.kicks_enabled && rig.nd.accp.microprogram_running,
          "running with kicks enabled before CPURES");
    rig.nd.sniff.latched = true; /* stands for a doorbell that has been identified */

    n = send_command(&rig, (uint8_t)NDBUS_ACCP_CPURES, replies);
    CHECK(n == 5 && rig.nd.last_nak_code == NDBUS_ACCP_ACCEPTED, "071B CPURES is acknowledged");
    CHECK(log.resets == 1, "071B calls reset_cpu_to_idle exactly once");
    CHECK(log.running_at_reset && log.kicks_at_reset,
          "the CPU is reset BEFORE the two cells are cleared, as the reference orders it");
    CHECK(!rig.nd.accp.microprogram_running, "CPURES clears 'microprogram running'");
    CHECK(!rig.nd.accp.kicks_enabled, "CPURES clears 'kicks enabled'");
    CHECK(rig.nd.accp.system_parameters_given, "CPURES KEEPS the system parameters given");
    CHECK(rig.nd.accp.parameter_pointer_given, "CPURES KEEPS the parameter pointer given");
    CHECK(rig.nd.parameter_pointer == PARAM_BLOCK, "and the pointer itself");
    CHECK(rig.nd.sniff.latched, "and it does not re-arm the doorbell sniff");

    /* The kept state is not just a flag: the read-back that naks 13 without
     * system parameters still returns the three words. */
    n = send_command(&rig, (uint8_t)NDBUS_ACCP_RSSYSPAR, replies);
    Message reply;
    int     index = 0;
    CHECK(n > 0 && next_message(replies, n, &index, &reply) && reply.length == 7 &&
              reply.payload[0] == 0x00u && reply.payload[1] == 0x01u && reply.payload[2] == 0x08u,
          "015B after CPURES still reads back LSYSPAR word 1 = 0x0108");

    /* ---- 241B MASTER CLEAR: station state first, then the CPU -------------- */
    (void)send_command_zeros(&rig, (uint8_t)NDBUS_ACCP_STARTMIC, 2, replies);
    (void)send_command(&rig, (uint8_t)NDBUS_ACCP_ENKICK, replies);
    CHECK(rig.nd.accp.kicks_enabled && rig.nd.accp.system_parameters_given,
          "state for the master clear to clear");
    int resets_before = log.resets;
    int inits_before  = log.inits;

    n = send_frame(&rig, emergency_frame(ST_ND5000, NDBUS_EMERGENCY_MASTER_CLEAR), replies);
    CHECK(n == 0, "241B has no reply frame");
    CHECK(log.resets == resets_before + 1, "241B calls reset_cpu_to_idle exactly once");
    CHECK(log.inits == inits_before, "and not apply_init_state");
    CHECK(!log.running_at_reset && !log.kicks_at_reset && !log.syspar_at_reset,
          "the station's own state was already reset when the CPU hook ran");

    /* The other two emergencies leave the CPU alone. */
    (void)send_frame(&rig, emergency_frame(ST_ND5000, NDBUS_EMERGENCY_CONTINUE_ACCP), replies);
    (void)send_frame(&rig, emergency_frame(ST_ND5000, NDBUS_EMERGENCY_TERMINATE_ACCP), replies);
    CHECK(log.resets == resets_before + 1, "242B and 244B do not reset the CPU");

    rig_down(&rig);

    /* ---- with no hooks installed the same frames still work ---------------- */
    rig_up(&rig, 4096u, ST_ND5000);
    n = send_command_zeros(&rig, (uint8_t)NDBUS_ACCP_STARTMIC, 2, replies);
    CHECK(n == 5, "066B with no hooks is acknowledged");
    n = send_command(&rig, (uint8_t)NDBUS_ACCP_CPURES, replies);
    CHECK(n == 5 && !rig.nd.accp.microprogram_running, "071B with no hooks still clears the cell");
    n = send_frame(&rig, emergency_frame(ST_ND5000, NDBUS_EMERGENCY_MASTER_CLEAR), replies);
    CHECK(n == 0 && rig.nd.master_clears == 1u, "241B with no hooks is still handled");
    rig_down(&rig);
}

/* ------------------------------------------------------------------------- */
/* 5. The model/version report after ENKICK                                   */
/* ------------------------------------------------------------------------- */

/*
 * OctobusND5000Station.cs lines 3275-3286 (the ENKICK arm: acknowledge, mailbox,
 * then the report) and SendMicroprogramModelReport, lines 3631-3670, whose last
 * line sends to _accpMessage.Source - the station ENKICK came from.
 *
 * BEFORE THE PORT the report frames carried the station's own number as their
 * destination, so the fabric delivered them back to the ND-5000 station itself and
 * the ENKICK sender got the five acknowledge frames and nothing else.
 */
static void test_model_report(void)
{
    printf("5. The model/version report after ENKICK\n");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];

    Rig rig;
    rig_up(&rig, 64u * 1024u, ST_ND5000);
    cs_allocate(&rig);
    cs_poke(&rig, 1u, 7u, 0x2E9Au); /* word 1 halfword 7: the LARG version */
    cs_poke(&rig, 7u, 7u, 0x0038u); /* word 7 halfword 7: the packed model byte */

    /* Word 1's high byte gives the report's destination OMD, (word1 >> 8) & 0x3F.
     * 0x0508 makes it 5, which is neither of the OMDs this station serves itself -
     * so a report that came back to the station could not be mistaken for traffic. */
    send_lsyspar(&rig, 0x0508u);
    (void)send_command_zeros(&rig, (uint8_t)NDBUS_ACCP_STARTMIC, 2, replies);
    CHECK(rig.nd.accp.microprogram_running, "the microprogram is running");

    unsigned long handled = rig.nd.messages_handled;
    rig.capture.count     = 0;
    int n = send_command(&rig, (uint8_t)NDBUS_ACCP_ENKICK, replies);

    CHECK(n == 15, "ENKICK answers 5 acknowledge frames plus 10 report frames");
    CHECK(rig.nd.model_reports == 1u, "one report was sent");
    CHECK(rig.nd.messages_handled == handled + 1u,
          "the station handled ENKICK and nothing else - the report did not come back to it");

    Message ack;
    Message report;
    int     index = 0;
    bool    ok    = next_message(replies, n, &index, &ack);
    CHECK(ok && ack.dest_omd == (uint8_t)NDBUS_ACCP_OMD && ack.source_omd == 3u &&
              ack.length == 1 && ack.payload[0] == 0x00u,
          "FIRST the acknowledge: one status byte, on the sender's OMD");
    ok = ok && next_message(replies, n, &index, &report);
    CHECK(ok && index == n, "THEN the report, and nothing after it");
    CHECK(ok && report.dest_omd == 5u, "the report is addressed to OMD (word1 >> 8) & 0x3F");
    CHECK(ok && report.source_omd == 4u, "from source OMD 4, not the ACCP's 3");
    CHECK(ok && report.length == 6 && report.payload[0] == 0x82u && report.payload[1] == 0x01u,
          "six bytes: FaultType 202B, ErrorReporter 1");
    CHECK(ok && report.payload[2] == 0x38u && report.payload[3] == 0x38u,
          "the model byte twice, out of control-store word 7");
    CHECK(ok && report.payload[4] == 0x2Eu && report.payload[5] == 0x9Au,
          "and the version out of control-store word 1");
    CHECK(ok && ack.station == ST_ND5000 && report.station == ST_ND5000,
          "every frame of both names the ND-5000 station as its sender");

    /* The report is not sent while the microprogram is stopped. */
    (void)send_command(&rig, (uint8_t)NDBUS_ACCP_STOPMIC, replies);
    n = send_command(&rig, (uint8_t)NDBUS_ACCP_ENKICK, replies);
    CHECK(n == 5 && rig.nd.model_reports == 1u,
          "ENKICK with the microprogram stopped is the bare acknowledge");

    rig_down(&rig);
}

/* ------------------------------------------------------------------------- */
/* 6. An EOMB for another OMD                                                 */
/* ------------------------------------------------------------------------- */

/*
 * OctobusND5000Station.cs lines 2525-2589: SOMB and EOMB are consumed by the ACCP
 * only for OMD 0 and OMD 3; for any other OMD both go to the microprogram and the
 * message the ACCP is collecting is left alone.
 *
 * BEFORE THE PORT an EOMB of ANY OMD closed the open message and ran it as an ACCP
 * command.
 */
static void test_eomb_of_another_omd(void)
{
    printf("6. An EOMB for another OMD\n");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
    uint16_t dest = (uint16_t)((uint16_t)ST_ND5000 << NDBUS_FRAME_STATION_SHIFT);

    Rig rig;
    rig_up(&rig, 4096u, ST_ND5000);

    /* Open an OMD-3 message carrying READSELFT and stop before its EOMB. */
    (void)send_frame(&rig, (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                                      NDBUS_FRAME_S_STARTSTOP | (uint16_t)NDBUS_ACCP_OMD),
                     replies);
    (void)send_frame(&rig, (uint16_t)(dest | 3u), replies); /* source OMD */
    (void)send_frame(&rig, (uint16_t)(dest | 1u), replies); /* byte count */
    (void)send_frame(&rig, (uint16_t)(dest | (uint16_t)NDBUS_ACCP_READSELFT), replies);
    CHECK(rig.nd.inbox.open && rig.nd.inbox.count == 3, "an OMD-3 message is being collected");

    /* The EOMB of OMD 5 is the microprogram's frame. */
    uint16_t eomb5 = (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE | 5u);
    int      n     = send_frame(&rig, eomb5, replies);
    CHECK(n == 0, "an EOMB for OMD 5 is not answered");
    CHECK(rig.nd.messages_handled == 0u, "and does not run the open message as a command");
    CHECK(rig.nd.inbox.open && rig.nd.inbox.count == 3, "the message stays open, untouched");

    /* Nor does a SOMB of OMD 5 restart it. */
    (void)send_frame(&rig, (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                                      NDBUS_FRAME_S_STARTSTOP | 5u),
                     replies);
    CHECK(rig.nd.inbox.open && rig.nd.inbox.count == 3, "a SOMB for OMD 5 leaves it alone too");

    /* Its own EOMB completes it. */
    n = send_frame(&rig, (uint16_t)(dest | NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                                    (uint16_t)NDBUS_ACCP_OMD),
                   replies);
    Message reply;
    int     index = 0;
    CHECK(n > 0 && next_message(replies, n, &index, &reply) && reply.length == 3,
          "the OMD-3 EOMB completes the message and READSELFT is answered");
    CHECK(rig.nd.messages_handled == 1u && rig.nd.last_command == (uint8_t)NDBUS_ACCP_READSELFT,
          "exactly once, as the command that was sent");

    rig_down(&rig);
}

/* ------------------------------------------------------------------------- */
/* 7. The patched control-store cells                                         */
/* ------------------------------------------------------------------------- */

/*
 * OctobusND5000Station.cs: LoadMmsPointersFromControlStore (1568-1644) and
 * DeriveCpuIdentityFromControlStore (1662-1705), called from the 066B/035B/036B
 * arm at lines 3396 and 3402; TryGetContextBlockAreaBase, lines 2181-2189.
 *
 * BEFORE THE PORT the cells were read only from ENKICK, and only after a mailbox
 * had been attached - so with START_MESS unpatched nothing was read at all.
 */
static void test_patched_cells(void)
{
    printf("7. The patched control-store cells\n");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];

    /* ---- read at the start, by all three start commands, with NO mailbox ---- */
    static const uint8_t starters[3]     = { (uint8_t)NDBUS_ACCP_STARTMIC,
                                             (uint8_t)NDBUS_ACCP_CONTMIC,
                                             (uint8_t)NDBUS_ACCP_RESTMIC };
    static const int     starter_args[3] = { 2, 0, 4 };
    for (int k = 0; k < 3; k++)
    {
        Rig rig;
        rig_up(&rig, 64u * 1024u, ST_ND5000);
        cs_allocate(&rig);

        /* START_MESS (026B) stays ZERO: there is no mailbox to find. */
        cs_poke_larg(&rig, 0x11u, 0x00000074u); /* 0o21 PSTBASE, an ND-500 page */
        cs_poke_larg(&rig, 0x10u, 0x0002A000u); /* 0o20 OFFSET, already bytes */
        cs_poke(&rig, 1u, 7u, 0x1234u);         /* word 1: microprogram version */
        cs_poke(&rig, 7u, 7u, 0x0038u);         /* word 7: model byte 0x38 */
        rig.nd.servicer.cpu_parameter = 0x0001u; /* so the model-8 value is seen to arrive */

        (void)send_command_zeros(&rig, starters[k], starter_args[k], replies);

        CHECK(rig.nd.accp.microprogram_running, "the start command was accepted");
        CHECK(rig.nd.start_mess == 0u, "no mailbox was located - START_MESS is unpatched");
        CHECK(rig.nd.pst_base == (0x74u << 11u),
              "PSTBASE was read at the start: page 164B shifted 11 = 0x3A000");
        CHECK(rig.nd.context_area == 0x0002A000u,
              "OFFSET was read at the start, all 32 bits, unshifted");
        CHECK(rig.nd.servicer.context_area_base == 0x0002A000u,
              "and handed to the servicer");
        CHECK(rig.nd.servicer.micro_version == 0x1234u,
              "the 3RMICV version came from control-store word 1");
        CHECK(rig.nd.servicer.cpu_parameter == 0x03E1u,
              "and the CPU parameter from the model byte in word 7");

        rig_down(&rig);
    }

    /* ---- the two cells are independent of each other ----------------------- */
    {
        Rig rig;
        rig_up(&rig, 64u * 1024u, ST_ND5000);
        cs_allocate(&rig);
        cs_poke_larg(&rig, 0x10u, 0x00012300u); /* OFFSET patched, PSTBASE left zero */
        (void)send_command_zeros(&rig, (uint8_t)NDBUS_ACCP_STARTMIC, 2, replies);
        CHECK(rig.nd.pst_base == 0u, "an unpatched PSTBASE leaves the segment table unknown");
        CHECK(rig.nd.context_area == 0x00012300u,
              "and does NOT stop the context block area from being read");
        rig_down(&rig);

        rig_up(&rig, 64u * 1024u, ST_ND5000);
        cs_allocate(&rig);
        cs_poke_larg(&rig, 0x11u, 0x00000002u); /* PSTBASE patched, OFFSET left zero */
        (void)send_command_zeros(&rig, (uint8_t)NDBUS_ACCP_STARTMIC, 2, replies);
        CHECK(rig.nd.pst_base == (2u << 11u), "a patched PSTBASE is read");
        CHECK(rig.nd.context_area == 0u, "with OFFSET unpatched the area stays unknown");
        rig_down(&rig);
    }

    /* ---- ENKICK reads START_MESS and SAMSON_CPU, and nothing else ---------- */
    {
        Rig rig;
        rig_up(&rig, 64u * 1024u, ST_ND5000);
        cs_allocate(&rig);
        cs_poke_larg(&rig, 0x16u, MBX_HEADER);
        cs_poke_larg(&rig, 0x15u, 1u);
        cs_poke_larg(&rig, 0x11u, 0x00000074u);
        cs_poke_larg(&rig, 0x10u, 0x0002A000u);

        (void)send_command(&rig, (uint8_t)NDBUS_ACCP_ENKICK, replies);
        CHECK(rig.nd.start_mess == MBX_HEADER, "ENKICK located the mailbox");
        CHECK(rig.nd.pst_base == 0u && rig.nd.context_area == 0u,
              "but did not read PSTBASE or OFFSET - those belong to the microprogram start");
        CHECK(rig.nd.servicer.context_area_base == 0u, "so the servicer has no area either");

        /* ---- the on-demand read of cell 0o20 ------------------------------- */
        uint32_t base = 0xDEADBEEFu;
        CHECK(ndbus_nd5000_try_get_context_block_area_base(&rig.nd, &base),
              "the on-demand read finds the patched cell");
        CHECK(base == 0x0002A000u, "and returns its 32-bit value");
        CHECK(rig.nd.context_area == 0x0002A000u,
              "recording it, because nothing had been recorded yet");

        /* It reads the CELL every time; a value already recorded is not replaced. */
        cs_poke_larg(&rig, 0x10u, 0x00030000u);
        CHECK(ndbus_nd5000_try_get_context_block_area_base(&rig.nd, &base) &&
                  base == 0x00030000u,
              "a re-patched cell is returned as it now reads");
        CHECK(rig.nd.context_area == 0x0002A000u, "while the recorded value is kept");

        cs_poke_larg(&rig, 0x10u, 0u);
        CHECK(!ndbus_nd5000_try_get_context_block_area_base(&rig.nd, &base) && base == 0u,
              "an unpatched cell is reported as not found, with 0");
        CHECK(!ndbus_nd5000_try_get_context_block_area_base(&rig.nd, NULL),
              "a NULL result pointer is refused");
        rig_down(&rig);

        /* No control store at all. */
        rig_up(&rig, 4096u, ST_ND5000);
        base = 0xDEADBEEFu;
        CHECK(!ndbus_nd5000_try_get_context_block_area_base(&rig.nd, &base) && base == 0u,
              "with no control store there is nothing to find");
        (void)send_command_zeros(&rig, (uint8_t)NDBUS_ACCP_STARTMIC, 2, replies);
        CHECK(rig.nd.accp.microprogram_running && rig.nd.pst_base == 0u &&
                  rig.nd.context_area == 0u,
              "and a start with no control store reads nothing and still starts");
        rig_down(&rig);
    }
}

/* ------------------------------------------------------------------------- */
/* 8. ECHO 017B                                                               */
/* ------------------------------------------------------------------------- */

/*
 * OctobusND5000Station.cs: no arm for 0x0F, so the canned acknowledge at line
 * 3493 answers it; AccpCommandGuards.MinimumParameterBytes gives it one byte.
 *
 * BEFORE THE PORT the reply was the acknowledge PLUS the echoed bytes.
 */
static void test_echo(void)
{
    printf("8. ECHO 017B\n");

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];

    Rig rig;
    rig_up(&rig, 4096u, ST_ND5000);

    static const uint8_t echo[4] = { (uint8_t)NDBUS_ACCP_ECHO, 2u, 0xAAu, 0x55u };
    int                  n       = send_accp(&rig, echo, 4, replies);
    Message              reply;
    int                  index = 0;
    CHECK(n == 5 && next_message(replies, n, &index, &reply),
          "ECHO with a count of 2 is answered with one five-frame message");
    CHECK(reply.length == 1 && reply.payload[0] == 0x00u,
          "which is the bare acknowledge: one status byte and no echoed data");
    CHECK(rig.nd.last_nak_code == NDBUS_ACCP_ACCEPTED && rig.nd.messacks == 1u,
          "counted as a Messack");

    /* The measured case: a count byte of zero answers the single byte 00. */
    static const uint8_t echo_zero[2] = { (uint8_t)NDBUS_ACCP_ECHO, 0u };
    n     = send_accp(&rig, echo_zero, 2, replies);
    index = 0;
    CHECK(n == 5 && next_message(replies, n, &index, &reply) && reply.length == 1 &&
              reply.payload[0] == 0x00u,
          "ECHO with a count of 0 answers 00, as measured on the real firmware");

    /* Without its count byte it is below the measured length: no reply at all. */
    n = send_command(&rig, (uint8_t)NDBUS_ACCP_ECHO, replies);
    CHECK(n == 0, "ECHO with no count byte gets no reply");

    rig_down(&rig);
}

/* ------------------------------------------------------------------------- */
/* 9. The mailbox configuration: fallback CPUNO and the window guard          */
/* ------------------------------------------------------------------------- */

/*
 * OctobusND5000Station.cs ConfigureMailboxFromControlStore, lines 1435-1466:
 *     int cpu = samsonCpu != 0 ? (int)samsonCpu : _cpuNumber;          (1453)
 *     if (header < _mpm.Start || ext + 16 >= _mpm.Start + _mpm.Size)   (1457)
 * with _cpuNumber = 1 from construction (949) and updated by ConfigureMailbox (1398).
 *
 * BEFORE THE PORT the fallback was derived from the station number (2 for 071B)
 * and the guard demanded that the whole 256-byte extension block fit.
 */
static void test_mailbox_configuration(void)
{
    printf("9. The mailbox configuration\n");

    /* ---- the fallback CPUNO is 1, whatever the station ---------------------- */
    {
        Rig rig;
        rig_up(&rig, 64u * 1024u, (uint8_t)(ST_ND5000 + 1u)); /* 071B */
        cs_allocate(&rig);
        CHECK(rig.nd.cpu_number == 1, "a new station's CPUNO is 1");

        cs_poke_larg(&rig, 0x16u, 0x2000u); /* START_MESS; SAMSON_CPU stays zero */
        CHECK(ndbus_nd5000_mailbox_from_control_store(&rig.nd), "the mailbox is configured");
        CHECK(rig.nd.samson_cpu == 0u, "SAMSON_CPU read as unpatched");
        CHECK(ndbus_mailbox_ext_base(&rig.nd.mailbox) == 0x2000u + 256u,
              "station 071B with SAMSON_CPU unpatched uses block 1, not block 2");
        CHECK(rig.nd.cpu_number == 1, "and its CPUNO is still 1");

        /* A patched SAMSON_CPU wins, and becomes the CPUNO the fallback uses next. */
        cs_poke_larg(&rig, 0x15u, 3u);
        CHECK(ndbus_nd5000_mailbox_from_control_store(&rig.nd), "configured again, SAMSON_CPU 3");
        CHECK(ndbus_mailbox_ext_base(&rig.nd.mailbox) == 0x2000u + (3u * 256u),
              "a patched SAMSON_CPU selects its own block");
        CHECK(rig.nd.cpu_number == 3, "and is remembered as the station's CPUNO");

        cs_poke_larg(&rig, 0x15u, 0u);
        cs_poke_larg(&rig, 0x16u, 0x3000u);
        CHECK(ndbus_nd5000_mailbox_from_control_store(&rig.nd), "configured a third time");
        CHECK(ndbus_mailbox_ext_base(&rig.nd.mailbox) == 0x3000u + (3u * 256u),
              "the fallback is the LAST configured CPUNO, 3 - not 1 and not the station's");

        rig_down(&rig);
    }

    /* ---- the guard: 17 bytes of the extension block, not all 256 ------------ */
    {
        Rig rig;
        rig_up(&rig, 0x1000u, ST_ND5000); /* a 4096-byte window */
        cs_allocate(&rig);

        /* ext = START_MESS + 256. Accepted while ext + 16 < 0x1000. */
        cs_poke_larg(&rig, 0x16u, 0x0E80u); /* ext = 0xF80: only 128 of its 256 bytes fit */
        CHECK(ndbus_nd5000_mailbox_from_control_store(&rig.nd),
              "a block whose first 17 bytes fit is accepted, though its 256 do not");
        CHECK(rig.nd.start_mess == 0x0E80u &&
                  ndbus_mailbox_ext_base(&rig.nd.mailbox) == 0x0F80u,
              "and configured where START_MESS says");
        CHECK(ndbus_mailbox_write_ext(&rig.nd.mailbox, NDBUS_MBX_X5ACT_WORD, 0x1234u) &&
                  ndbus_pool_read16(&rig.pool, 0x0F80u + EXT_X5ACT_BYTE) == 0x1234u,
              "the block is usable: X5ACT is written at ext + 0x0A");

        cs_poke_larg(&rig, 0x16u, 0x0EEEu); /* ext + 16 = 0xFFE, the last accepted even base */
        CHECK(ndbus_nd5000_mailbox_from_control_store(&rig.nd),
              "ext + 16 just below the window end is accepted");

        cs_poke_larg(&rig, 0x16u, 0x0EF0u); /* ext + 16 = 0x1000 = the window end */
        CHECK(!ndbus_nd5000_mailbox_from_control_store(&rig.nd),
              "ext + 16 equal to the window end is refused");
        CHECK(rig.nd.start_mess == 0x0EEEu, "and a refusal changes nothing");

        /* The window's ND-100 base does not change the answer: both sides of the
         * comparison carry it. */
        ndbus_servicer_set_nd100_window_base(&rig.nd.servicer, 0x00420000u);
        cs_poke_larg(&rig, 0x16u, 0x0E80u);
        CHECK(ndbus_nd5000_mailbox_from_control_store(&rig.nd) && rig.nd.start_mess == 0x0E80u,
              "with the window at ND-100 byte 0x420000 the same base is accepted");
        cs_poke_larg(&rig, 0x16u, 0x0EF0u);
        CHECK(!ndbus_nd5000_mailbox_from_control_store(&rig.nd),
              "and the same base is refused");

        rig_down(&rig);
    }

    /* ---- the first half of the guard: a header address that wraps 32 bits ---- */
    {
        Rig rig;
        rig_up(&rig, 64u * 1024u, ST_ND5000);
        cs_allocate(&rig);
        cs_poke_larg(&rig, 0x16u, 0x2000u);

        /* Window start 0xFFFFE000 plus START_MESS 0x2000 is 0 in 32 bits, which is
         * below the window start. The second half of the guard would let it
         * through - the wrapped extension block address is small - so this is the
         * first half alone. The address is not a realistic one; the test is of the
         * comparison the reference makes. */
        ndbus_servicer_set_nd100_window_base(&rig.nd.servicer, 0xFFFFE000u);
        CHECK(!ndbus_nd5000_mailbox_from_control_store(&rig.nd),
              "a header address that wraps below the window start is refused");
        CHECK(rig.nd.start_mess == 0u, "and nothing is configured");

        ndbus_servicer_set_nd100_window_base(&rig.nd.servicer, 0u);
        CHECK(ndbus_nd5000_mailbox_from_control_store(&rig.nd) && rig.nd.start_mess == 0x2000u,
              "the same START_MESS in a window that does not wrap is accepted");

        rig_down(&rig);
    }
}

/* ------------------------------------------------------------------------- */
/* 10. ND-500 address zero                                                    */
/* ------------------------------------------------------------------------- */

static int send_vparp(Rig *rig, uint32_t *echo)
{
    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
    Message  reply;
    int      index = 0;
    int      n     = send_command(rig, (uint8_t)NDBUS_ACCP_VPARP, replies);

    *echo = 0u;
    if (n <= 0 || !next_message(replies, n, &index, &reply) || reply.length != 5 ||
        reply.payload[0] != 0x00u)
    {
        return -1;
    }
    *echo = ((uint32_t)reply.payload[1] << 24u) | ((uint32_t)reply.payload[2] << 16u) |
            ((uint32_t)reply.payload[3] << 8u) | (uint32_t)reply.payload[4];
    return 0;
}

/*
 * OctobusND5000Station.cs: Nd500ZeroByte / ResolveNd500Byte (252-269),
 * CalibrateNd500Zero (3885-3929), SendVparpEcho (3955-3961),
 * ServiceControlStoreWrite (4024) and ServiceControlStoreReadback (4139).
 *
 * BEFORE THE PORT the LPARP pointer was used as a pool offset from byte 0, so on a
 * machine whose ADRZERO is not the window base VPARP read the wrong cell and the
 * control-store load took its staging block from the wrong place.
 */
static void test_nd500_address_zero(void)
{
    printf("10. ND-500 address zero\n");

    const uint32_t window  = 0x00420000u; /* the window's ND-100 byte address */
    const uint32_t pointer = 0x00000800u; /* what LPARP delivers on the live machine */
    uint32_t       echo    = 0u;

    /* ---- calibrated from the pattern, then used by VPARP, LOCSM and DUCS ---- */
    {
        Rig rig;
        rig_up(&rig, 64u * 1024u, ST_ND5000);
        ndbus_servicer_set_nd100_window_base(&rig.nd.servicer, window);

        /* SINTRAN's parameter area is 0x1000 bytes further up than "window base +
         * pointer": ADRZERO is ND-100 byte 0x421000, page 1020B. */
        const uint32_t real_area = 0x1800u; /* pool offset of ADRZERO + pointer */
        (void)ndbus_pool_write32(&rig.pool, pointer, 0x11112222u); /* the wrong cell */
        (void)ndbus_pool_write32(&rig.pool, real_area, NDBUS_ACCP_VPARP_TEST_PATTERN);

        send_lparp(&rig, pointer);
        CHECK(rig.nd.nd500_zero_byte == 0u, "ADRZERO starts uncalibrated");
        CHECK(send_vparp(&rig, &echo) == 0, "VPARP is answered with a Messack and four bytes");
        CHECK(echo == NDBUS_ACCP_VPARP_TEST_PATTERN,
              "VPARP echoes the test pattern, found at ADRZERO + pointer");
        CHECK(rig.nd.nd500_zero_byte == window + 0x1000u,
              "ADRZERO was calibrated to the ND-100 address the pattern implies");

        s_log_lines = 0;
        CHECK(send_vparp(&rig, &echo) == 0 && echo == NDBUS_ACCP_VPARP_TEST_PATTERN,
              "a second VPARP reads the same cell");
        CHECK(s_log_lines == 0, "without calibrating again");

        /* LOCSM takes its staging block from the resolved address. A decoy block at
         * the unresolved offset names a different control-store address. */
        (void)ndbus_pool_write16(&rig.pool, real_area, 1u);       /* N = 1 */
        (void)ndbus_pool_write16(&rig.pool, real_area + 2u, 5u);  /* control-store word 5 */
        (void)ndbus_pool_write16(&rig.pool, pointer, 1u);
        (void)ndbus_pool_write16(&rig.pool, pointer + 2u, 9u);    /* the decoy: word 9 */
        for (uint32_t i = 0u; i < 8u; i++)
        {
            (void)ndbus_pool_write16(&rig.pool, real_area + 4u + (i * 2u), (uint16_t)(0xB000u + i));
            (void)ndbus_pool_write16(&rig.pool, pointer + 4u + (i * 2u), (uint16_t)(0xD000u + i));
        }
        uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
        (void)send_command(&rig, (uint8_t)NDBUS_ACCP_LOCSM, replies);
        CHECK(rig.nd.control_store != NULL, "LOCSM allocated the control store");
        bool loaded = (rig.nd.control_store != NULL);
        bool decoy  = false;
        for (uint32_t i = 0u; loaded && i < 8u; i++)
        {
            if (rig.nd.control_store[(5u * 8u) + i] != (uint16_t)(0xB000u + i))
            {
                loaded = false;
            }
            if (rig.nd.control_store[(9u * 8u) + i] != 0u)
            {
                decoy = true;
            }
        }
        CHECK(loaded, "LOCSM loaded the microword staged at ADRZERO + pointer");
        CHECK(!decoy, "and not the block at pool offset 'pointer'");

        /* DUCS writes its dump at the resolved address as well. (That it WRITES
         * the dump, where the reference answers the ND-100's reads instead, is a
         * stated difference and is not what this checks.) */
        (void)ndbus_pool_write16(&rig.pool, real_area, 1u);
        (void)ndbus_pool_write16(&rig.pool, real_area + 2u, 5u);
        (void)send_command(&rig, (uint8_t)NDBUS_ACCP_DUCS, replies);
        bool     dumped = true;
        uint32_t sum    = 0u;
        for (uint32_t i = 0u; i < 8u; i++)
        {
            if (ndbus_pool_read16(&rig.pool, real_area + (i * 2u)) != (uint16_t)(0xB000u + i))
            {
                dumped = false;
            }
            sum = (sum + 0xB000u + i) & 0xFFFFu;
        }
        CHECK(dumped, "DUCS dumped the microword at ADRZERO + pointer");
        CHECK(ndbus_pool_read16(&rig.pool, real_area + 16u) == (uint16_t)sum,
              "with the checksum addend straight after it");
        CHECK(ndbus_pool_read16(&rig.pool, pointer + 2u) == 9u,
              "and left the block at pool offset 'pointer' alone");

        rig_down(&rig);
    }

    /* ---- already right: the pattern is at window base + pointer ------------- */
    {
        Rig rig;
        rig_up(&rig, 64u * 1024u, ST_ND5000);
        ndbus_servicer_set_nd100_window_base(&rig.nd.servicer, window);
        (void)ndbus_pool_write32(&rig.pool, pointer, NDBUS_ACCP_VPARP_TEST_PATTERN);
        send_lparp(&rig, pointer);
        CHECK(send_vparp(&rig, &echo) == 0 && echo == NDBUS_ACCP_VPARP_TEST_PATTERN,
              "with ADRZERO at the window base VPARP echoes the pattern directly");
        CHECK(rig.nd.nd500_zero_byte == 0u, "and nothing is calibrated");
        rig_down(&rig);
    }

    /* ---- refused: a base that is not a whole ND-100 page -------------------- */
    {
        Rig rig;
        rig_up(&rig, 64u * 1024u, ST_ND5000);
        ndbus_servicer_set_nd100_window_base(&rig.nd.servicer, window);
        (void)ndbus_pool_write32(&rig.pool, pointer, 0x11112222u);
        (void)ndbus_pool_write32(&rig.pool, 0x1802u, NDBUS_ACCP_VPARP_TEST_PATTERN);
        send_lparp(&rig, pointer);
        CHECK(send_vparp(&rig, &echo) == 0 && echo == 0x11112222u,
              "a pattern 2 bytes off a page boundary is not adopted");
        CHECK(rig.nd.nd500_zero_byte == 0u, "and ADRZERO stays uncalibrated");
        rig_down(&rig);
    }

    /* ---- refused: two page-aligned candidates ------------------------------- */
    {
        Rig rig;
        rig_up(&rig, 64u * 1024u, ST_ND5000);
        ndbus_servicer_set_nd100_window_base(&rig.nd.servicer, window);
        (void)ndbus_pool_write32(&rig.pool, pointer, 0x11112222u);
        (void)ndbus_pool_write32(&rig.pool, 0x1800u, NDBUS_ACCP_VPARP_TEST_PATTERN);
        (void)ndbus_pool_write32(&rig.pool, 0x2800u, NDBUS_ACCP_VPARP_TEST_PATTERN);
        send_lparp(&rig, pointer);
        CHECK(send_vparp(&rig, &echo) == 0 && echo == 0x11112222u,
              "two equally plausible bases are refused rather than picked between");
        CHECK(rig.nd.nd500_zero_byte == 0u, "and ADRZERO stays uncalibrated");
        rig_down(&rig);
    }

    /* ---- refused: a pattern that would put ADRZERO below zero --------------- */
    {
        Rig rig;
        rig_up(&rig, 64u * 1024u, ST_ND5000); /* window base 0 */
        (void)ndbus_pool_write32(&rig.pool, 0x2000u, 0x33334444u);
        (void)ndbus_pool_write32(&rig.pool, 0x1000u, NDBUS_ACCP_VPARP_TEST_PATTERN);
        send_lparp(&rig, 0x2000u);
        CHECK(send_vparp(&rig, &echo) == 0 && echo == 0x33334444u,
              "a pattern below the pointer cannot be the parameter area");
        CHECK(rig.nd.nd500_zero_byte == 0u, "and ADRZERO stays uncalibrated");
        rig_down(&rig);
    }

    /* ---- a pool that is the ND-100's whole view: window base 0 -------------- */
    {
        Rig rig;
        rig_up(&rig, 64u * 1024u, ST_ND5000);
        (void)ndbus_pool_write32(&rig.pool, 0x1000u, NDBUS_ACCP_VPARP_TEST_PATTERN);
        send_lparp(&rig, pointer);
        CHECK(send_vparp(&rig, &echo) == 0 && echo == NDBUS_ACCP_VPARP_TEST_PATTERN,
              "with no window base the calibration still finds the pattern");
        CHECK(rig.nd.nd500_zero_byte == 0x0800u, "ADRZERO is byte 0x800, one ND-100 page up");
        rig_down(&rig);
    }
}

int main(void)
{
    printf("ND-5000 octobus station against its C# reference\n");
    printf("=================================================\n\n");

    test_kicks();
    test_idle_gate();
    test_cpu_hooks_and_cpures();
    test_model_report();
    test_eomb_of_another_omd();
    test_patched_cells();
    test_echo();
    test_mailbox_configuration();
    test_nd500_address_zero();

    printf("\n%d check(s), %d failed\n", s_checks, s_failed);
    if (s_failed != 0)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
