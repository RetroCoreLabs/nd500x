/*
 * ndbus_nd5000.h - the ND-5000's octobus station, and the shared-memory doorbell
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * One of these sits at each occupied ND-5000 slot, octal 70B..76B (decimal
 * 56..62) - ND-05.020.01 T329. It is the ACCP as the bus sees it: the ND-120
 * sends multibyte messages to OMD 3, this reassembles them, runs the guard table
 * in ndbus_accp.h and answers Messack or Messnak.
 *
 * WHAT TRAVELS WHERE. T99: "The octobus is used for messages to initiate
 * operations. As a general rule, these operations work on data in shared memory
 * in the MFbus system. Thus, the octobus is normally not used to transport data.
 * The only exception is during debugging and testing." So the octobus carries
 * the command; the DATA is in the pool, and the doorbell below is how the two
 * sides tell each other it is there.
 */

#ifndef NDBUS_ND5000_H
#define NDBUS_ND5000_H

#include "ndbus_accp.h"
#include "ndbus_multibyte.h"
#include "ndbus_octobus.h"
#include "ndbus_pool.h"

/*
 * THE X5ACT DOORBELL, AND THE THRESHOLD THAT CANNOT BE MET.
 *
 * Self-discovery watches shared memory for a cell that goes 0xFFFF -> 0, which
 * is the signature of the guest ringing the doorbell.
 *
 * `[V-SRC]` Per the byte-verified microcode reference, XMSINIT initialises X5ACT
 * to -1, and the microcode re-arms it by writing **1**, not -1 - microword
 * 0o24722's literal 1 at IDLE_2, which runs BEFORE the message is consumed. So
 * the genuine doorbell produces exactly ONE -1 to 0 transition per XMSINIT;
 * every ring after that is 1 -> 0 and does not match the signature.
 *
 * A REPEAT THRESHOLD OF 2 OR MORE THEREFORE CANNOT BE MET. The sniff never
 * latches, and NOTHING ERRORS - the machine simply sits looking idle. That is
 * the worst shape a setting can have: it does not fail, it quietly answers a
 * different question. The threshold is kept and kept settable, because
 * suppressing self-discovery on purpose is a legitimate thing to want while
 * chasing a mis-latch; setting 2 or more logs the warning above at the moment it
 * is set, rather than leaving someone to work it out from an idle machine.
 *
 * The older claim that the real doorbell repeats because a watchdog rings it was
 * reasoning about what a watchdog ought to do, never an observation.
 */
#define NDBUS_X5ACT_LATCH_ON_FIRST 1u

typedef struct NdbusDoorbellSniff
{
    uint32_t candidate_offset; /* pool offset being watched */
    uint32_t transitions;      /* 0xFFFF -> 0 transitions seen at it */
    uint32_t threshold;        /* 0 or 1 = latch on the first (see above) */
    bool     latched;
    bool     have_candidate;
} NdbusDoorbellSniff;

typedef struct NdbusNd5000 NdbusNd5000;

struct NdbusNd5000
{
    NdbusStation        station;   /* registered on the fabric; MUST be first */
    NdbusPool          *pool;      /* the shared MFbus memory */
    const NdbusHostOps *host;
    const NdbusCpuOps  *cpu;       /* the ND-5000 this station fronts; may be NULL */

    NdbusAccpState      accp;
    NdbusMultibyte      inbox;     /* the OMD-3 message being reassembled */
    NdbusDoorbellSniff  sniff;

    /* LPARP's 4-byte pointer to the parameter area in MFbus memory, as a pool
     * BYTE offset. VPARP reads the 32-bit word there and echoes it back - the
     * one ACCP command a canned Messack cannot satisfy. */
    uint32_t            parameter_pointer;

    /* Diagnostics, so a test can say what the station did rather than infer it. */
    unsigned long       messages_handled;
    unsigned long       messacks;
    unsigned long       messnaks;
    uint8_t             last_command;
    int                 last_nak_code;   /* NDBUS_ACCP_ACCEPTED when the last was a Messack */
};

/*
 * Bring one ND-5000 station up at `station_number`, which must be 70B..76B.
 * Returns false for a station number outside that range - a station number is
 * configuration, and an ND-5000 answering at, say, 10B would collide with a SCSI
 * controller rather than fail visibly.
 *
 * Does NOT register on a fabric; call ndbus_fabric_register(&nd->station).
 */
bool ndbus_nd5000_init(NdbusNd5000 *nd, uint8_t station_number, NdbusPool *pool,
                       const NdbusHostOps *host, const NdbusCpuOps *cpu);

/* CPURES: back to the state a cold ACCP is in. The pool is NOT touched - it is
 * shared memory and the ND-100 owns its contents. */
void ndbus_nd5000_reset(NdbusNd5000 *nd);

/*
 * Offer one shared-memory word write to the doorbell sniff.
 *
 * Call it with the pool offset and the value being written. Returns true on the
 * write that LATCHES the doorbell. Once latched, the offset is in
 * `sniff.candidate_offset` and `sniff.latched` is set.
 */
bool ndbus_nd5000_sniff_write16(NdbusNd5000 *nd, uint32_t offset, uint16_t value);

/* Set the repeat threshold. Logs the warning above through host->log when a
 * value of 2 or more is set, because such a value can never be met. */
void ndbus_nd5000_set_sniff_threshold(NdbusNd5000 *nd, uint32_t threshold);

#endif /* NDBUS_ND5000_H */
