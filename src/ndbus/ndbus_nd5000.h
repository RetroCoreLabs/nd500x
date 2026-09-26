/**
 * @file ndbus_nd5000.h
 * @brief The ND-5000's octobus station, and the shared-memory doorbell.
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

/**
 * @brief Latch the doorbell on the FIRST matching transition.
 *
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

/** @brief State of the shared-memory doorbell sniff. */
typedef struct NdbusDoorbellSniff
{
    uint32_t candidate_offset; /**< pool offset being watched */
    uint32_t transitions;      /**< 0xFFFF -> 0 transitions seen at it */
    uint32_t threshold;        /**< 0 or 1 = latch on the first (see above) */
    bool     latched;          /**< true once the doorbell has been identified */
    bool     have_candidate;   /**< true once an offset is being watched */
} NdbusDoorbellSniff;

typedef struct NdbusNd5000 NdbusNd5000;

/** @brief One ND-5000 as the octobus sees it: its ACCP station plus its state. */
struct NdbusNd5000
{
    NdbusStation        station;   /**< registered on the fabric; MUST be first */
    NdbusPool          *pool;      /**< the shared MFbus memory */
    const NdbusHostOps *host;      /**< host callbacks for logging; may be NULL */
    const NdbusCpuOps  *cpu;       /**< the ND-5000 this station fronts; may be NULL */

    NdbusAccpState      accp;      /**< the ACCP guard state, see ndbus_accp.h */
    NdbusMultibyte      inbox;     /**< the OMD-3 message being reassembled */
    NdbusDoorbellSniff  sniff;     /**< the shared-memory doorbell sniff */

    /** LPARP's 4-byte pointer to the parameter area in MFbus memory, as a pool
     * BYTE offset. VPARP reads the 32-bit word there and echoes it back - the
     * one ACCP command a canned Messack cannot satisfy. */
    uint32_t            parameter_pointer;

    /* Diagnostics, so a test can say what the station did rather than infer it. */
    unsigned long       messages_handled; /**< complete OMD-3 messages handled */
    unsigned long       messacks;         /**< Messack replies sent */
    unsigned long       messnaks;         /**< Messnak replies sent */
    uint8_t             last_command;     /**< command byte of the last message */
    int                 last_nak_code;    /**< NDBUS_ACCP_ACCEPTED when the last was a Messack */

    /**
     * True while the ACCP program sits in its idle loop, which is where a
     * terminate (244B) puts it and where a master clear (241B) or a continue
     * (242B) takes it out again. A terminated ACCP still answers ACCP command
     * messages - only the microprogram is stopped - so this gates kicks, not
     * commands.
     */
    bool                accp_idle;

    /* Emergency counters, so a test can say what arrived rather than infer it. */
    unsigned long       master_clears;    /**< 241B emergency frames handled */
    unsigned long       continues;        /**< 242B emergency frames handled */
    unsigned long       terminates;       /**< 244B emergency frames handled */
    uint8_t             last_emergency;   /**< information byte of the last one */
};

/** @brief Emergency 241B: master clear - resets the ACCP and the ND-5000 CPU. */
#define NDBUS_EMERGENCY_MASTER_CLEAR   0xA1u
/** @brief Emergency 242B: continue ACCP - leave the idle loop and start up. */
#define NDBUS_EMERGENCY_CONTINUE_ACCP  0xA2u
/** @brief Emergency 244B: terminate ACCP - enter the idle loop, stop the microprogram. */
#define NDBUS_EMERGENCY_TERMINATE_ACCP 0xA4u

/**
 * @brief Bring one ND-5000 station up at `station_number`.
 *
 * Does NOT register on a fabric; call ndbus_fabric_register(&nd->station).
 *
 * @param nd             The station to initialise.
 * @param station_number The octobus station, which must be 70B..76B (decimal
 *                       56..62).
 * @param pool           The shared MFbus memory.
 * @param host           Host callbacks for logging; may be NULL.
 * @param cpu            The ND-5000 this station fronts; may be NULL.
 * @return true when initialised. false for a station number outside 70B..76B -
 *         a station number is configuration, and an ND-5000 answering at, say,
 *         10B would collide with a SCSI controller rather than fail visibly.
 */
bool ndbus_nd5000_init(NdbusNd5000 *nd, uint8_t station_number, NdbusPool *pool,
                       const NdbusHostOps *host, const NdbusCpuOps *cpu);

/**
 * @brief CPURES: back to the state a cold ACCP is in.
 *
 * The pool is NOT touched - it is shared memory and the ND-100 owns its
 * contents.
 *
 * @param nd The station to reset.
 * @return Nothing. Cannot fail.
 */
void ndbus_nd5000_reset(NdbusNd5000 *nd);

/**
 * @brief Offer one shared-memory word write to the doorbell sniff, BEFORE it lands.
 *
 * CALL ORDER IS PART OF THE CONTRACT, and the name says so because getting it
 * wrong is silent. The signature the sniff looks for is the TRANSITION
 * 0xFFFF -> 0, so the function reads the cell to learn the PREVIOUS value. Call
 * it after the write has already landed and the previous value it reads is the
 * new one - no transition is ever seen, the sniff never latches, and the machine
 * sits looking idle.
 *
 * That is the same failure mode as setting an unreachable repeat threshold,
 * reached by a completely different route, which is why the order is in the name
 * rather than only in a comment.
 *
 * This function does NOT perform the write. The caller still does that.
 *
 * Call it with the pool offset and the value being written. Once latched, the
 * offset is in `sniff.candidate_offset` and `sniff.latched` is set.
 *
 * @param nd     The station whose sniff is being offered the write.
 * @param offset The pool byte offset being written.
 * @param value  The 16-bit value being written there.
 * @return true on the write that LATCHES the doorbell, false on every other
 *         write - including writes that advance the transition count without
 *         reaching the threshold. false is not an error.
 */
bool ndbus_nd5000_sniff_before_write16(NdbusNd5000 *nd, uint32_t offset, uint16_t value);

/**
 * @brief Set the repeat threshold for the doorbell sniff.
 *
 * Logs the warning in the NDBUS_X5ACT_LATCH_ON_FIRST comment above through
 * host->log when a value of 2 or more is set, because such a value can never be
 * met.
 *
 * @param nd        The station.
 * @param threshold The number of 0xFFFF -> 0 transitions required to latch; 0 or
 *                  1 means latch on the first.
 * @return Nothing. A value of 2 or more is accepted, not refused, and is
 *         reported only by that log line.
 */
void ndbus_nd5000_set_sniff_threshold(NdbusNd5000 *nd, uint32_t threshold);

#endif /* NDBUS_ND5000_H */
