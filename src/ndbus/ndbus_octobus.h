/**
 * @file ndbus_octobus.h
 * @brief The octobus fabric: stations, frames and routing.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * THE BUS
 *
 * ND-05.020.01 T99: "The octobus is a serial, self-arbitrating bus used to
 * transfer messages between up to 62 devices. ... The octobus is used for
 * messages to initiate operations. As a general rule, these operations work on
 * data in shared memory in the MFbus system. Thus, the octobus is normally not
 * used to transport data. The only exception is during debugging and testing."
 * One byte takes 8 us at the maximum speed of 4 MHz.
 *
 * So: the DATA lives in the pool (ndbus_pool.h); the octobus only says "go".
 *
 * STATION NUMBERS ARE OCTAL IN EVERY ND DOCUMENT and DECIMAL on the wire, where
 * the field is 6 bits. ND-05.020.01 T329:
 *
 *     1       ND-120 CPU
 *     2 - 7   MFbus controllers
 *    10 - 13  SCSI controllers (disk)
 *    14 - 15  Matra VME
 *    16 - 17  Multifunction communication
 *    20       Hyperchannel
 *    21 - 23  FDDI (fibernet)
 *    24 - 27  FPS-5000
 *    30 - 33  Graphic controller
 *    34 - 67  Free for expansion
 *    70 - 76  ND-5000 CPU      <-- 56..62 decimal, SEVEN slots
 *
 * Station 0 and 77B (63) are not in the table and are refused here. RetroCore
 * records that an earlier version of its enum used the octal digits as decimal
 * literals (ND5000_CPU = 70), which does not fit in 6 bits; the constants below
 * are decimal and the octal spelling is in the comment beside each one.
 *
 * MASTER: T330. "If XRFO is not pulsing, indicating that no MASTER is selected,
 * the stations connected to the octobus automatically start to assign a MASTER.
 * The one with the lowest station number ends up as the MASTER and starts
 * transmitting the refresh signal (XRFO)."
 *
 * ARBITRATION: T330. "Each requesting station goes on transmitting until it
 * receives a '1' while transmitting a '0' itself. Then it ceases transmitting,
 * waits until the current frame is finished, and then starts again. At the time
 * a station gives up, its priority is incremented."
 *
 * FRAME: T331, start bit + 30 bits + stop bit, fields Priority / Destination /
 * C / B / Source / Information / Parity / Ack; the whole protocol is Appendix 2.
 * The 16-bit word below is the SOFTWARE view of a frame, which is what both
 * emulators exchange - not the 32-bit serial line format.
 */

#ifndef NDBUS_OCTOBUS_H
#define NDBUS_OCTOBUS_H

#include "ndbus_types.h"

/* ---- station numbers (DECIMAL; the octal spelling is in each comment) ------ */

/** @brief ND-120 CPU station, octal 1B. */
#define NDBUS_STATION_ND120_CPU     1u   /* 1B   */
/** @brief First MFbus controller station, octal 2B. */
#define NDBUS_STATION_MFBUS_FIRST   2u   /* 2B   */
/** @brief Last MFbus controller station, octal 7B. */
#define NDBUS_STATION_MFBUS_LAST    7u   /* 7B   */
/** @brief First ND-5000 CPU station, octal 70B. */
#define NDBUS_STATION_ND5000_FIRST 56u   /* 70B  */
/** @brief Last ND-5000 CPU station, octal 76B. */
#define NDBUS_STATION_ND5000_LAST  62u   /* 76B  */

/** @brief Highest legal station. 0 and 63 (77B) are illegal and are refused. */
#define NDBUS_STATION_MAX          62u   /* 76B  */

/** @brief One slot per 6-bit station number. */
#define NDBUS_STATION_SLOTS        64u

/** @brief Seven ND-5000 slots, 70B..76B. */
#define NDBUS_ND5000_MAX_CPUS      (NDBUS_STATION_ND5000_LAST - NDBUS_STATION_ND5000_FIRST + 1u)

/* ---- the software view of a frame ------------------------------------------
 *
 *  15 14 | 13 ........ 8 | 7  6  5  4 | 3 ...... 0
 *   C  B | DEST / SOURCE | E  K  M  S | code
 *
 * Bits 13-8 carry the DESTINATION when a frame is SENT and the SOURCE when it is
 * RECEIVED. The fabric performs that rewrite on delivery, exactly as the real
 * bus hardware presents frames to the receiving station (Appendix 2, 2.5).
 */

/** @brief C bit: control message, vs data. */
#define NDBUS_FRAME_C_CONTROL    ((uint16_t)(1u << 15u)) /* control message, vs data */
/** @brief B bit: addressed to a station TYPE (broadcast), vs one station. */
#define NDBUS_FRAME_B_BROADCAST  ((uint16_t)(1u << 14u)) /* to a station TYPE, vs one station */
/** @brief E bit: emergency message. */
#define NDBUS_FRAME_E_EMERGENCY  ((uint16_t)(1u << 7u))
/** @brief K bit: kick message. */
#define NDBUS_FRAME_K_KICK       ((uint16_t)(1u << 6u))
/** @brief M bit: this frame is part of a multibyte message. */
#define NDBUS_FRAME_M_MULTIBYTE  ((uint16_t)(1u << 5u))
/** @brief S bit: 1 = start of a multibyte message, 0 = stop/end. */
#define NDBUS_FRAME_S_STARTSTOP  ((uint16_t)(1u << 4u))  /* 1 = start, 0 = stop/end */

/** @brief Bit position of the 6-bit destination/source station field. */
#define NDBUS_FRAME_STATION_SHIFT 8
/** @brief Mask of the 6-bit destination/source station field. */
#define NDBUS_FRAME_STATION_MASK  ((uint16_t)(0x3Fu << NDBUS_FRAME_STATION_SHIFT))
/** @brief Mask of the information byte (bits 7-0). */
#define NDBUS_FRAME_DATA_MASK     ((uint16_t)0x00FFu)

/**
 * @brief Mask of the 4-bit code field (bits 3-0).
 *
 * Bits 3-0: the CODE field - emergency code, kick number, ident number or
 * OMD/CMD number, depending on the message type. ND-05.017.01 section 3.3.1
 * splits the information byte as E K M S | code(3..0) and states that CMD
 * numbers run 0 to 15, which pins the field at four bits.
 */
#define NDBUS_FRAME_CODE_MASK     ((uint16_t)0x000Fu)

/**
 * @brief Extract the 6-bit station field from a frame.
 * @param frame The 16-bit software view of a frame.
 * @return The station number in bits 13-8: the DESTINATION in a frame about to
 *         be sent, the SOURCE in a frame as received.
 */
static inline uint8_t ndbus_frame_station(uint16_t frame)
{
    return (uint8_t)((frame >> NDBUS_FRAME_STATION_SHIFT) & 0x3Fu);
}

/**
 * @brief Replace the 6-bit station field in a frame.
 * @param frame   The 16-bit software view of a frame.
 * @param station The station number to write; only the low 6 bits are used.
 * @return The frame with bits 13-8 replaced. No error return: an out-of-range
 *         station is masked to 6 bits, and legality is decided by the fabric.
 */
static inline uint16_t ndbus_frame_set_station(uint16_t frame, uint8_t station)
{
    return (uint16_t)((frame & (uint16_t)~NDBUS_FRAME_STATION_MASK) |
                      (uint16_t)(((uint16_t)station & 0x3Fu) << NDBUS_FRAME_STATION_SHIFT));
}

/* ---- stations -------------------------------------------------------------- */

/**
 * @brief The most frames one station may answer with in a single exchange.
 *
 * A reply longer than this is a station bug, not a bus condition, so the fabric
 * asserts the bound rather than growing a buffer on the routing path.
 */
/*
 * A reply is a WHOLE MULTIBYTE MESSAGE, not a frame: SOMB, the source OMD, the
 * byte count, the payload, EOMB - four frames of envelope plus one per payload
 * byte. Eight was too few the moment the envelope was built correctly (a VPARP
 * echo is ack + four bytes = nine frames), and a reply that does not fit is a
 * reply the receiver reads as a different message. Sixteen is the receiving
 * card's own FIFO depth, so nothing longer could be delivered whole anyway.
 */
#define NDBUS_MAX_REPLY_FRAMES 16

typedef struct NdbusStation NdbusStation;

/**
 * @brief Handle a frame addressed to this station.
 *
 * `frame` arrives in RECEIVE format - bits 13-8 already rewritten to the sender -
 * and `source_station` is the same value, spelled out so a handler need not
 * unpack it.
 *
 * Write reply frames into `replies` (at most NDBUS_MAX_REPLY_FRAMES) and return
 * how many. Return 0 for "accepted, nothing to say".
 *
 * @param station        The station receiving the frame.
 * @param frame          The frame in RECEIVE format.
 * @param source_station The sending station number.
 * @param replies        Buffer of at least NDBUS_MAX_REPLY_FRAMES frames.
 * @return The number of reply frames written, 0 for "accepted, nothing to say".
 *         A handler does not report a timeout; absence of a station is the
 *         fabric's business, not a handler's.
 */
typedef int (*NdbusStationHandler)(NdbusStation *station, uint16_t frame, uint8_t source_station,
                                   uint16_t *replies);

/** @brief One station attached to the octobus fabric. */
struct NdbusStation
{
    uint8_t             number;   /**< 1..62; 0 and 63 are illegal */
    const char         *type;     /**< human-readable, for logs; never NULL */
    NdbusStationHandler handle;   /**< may be NULL: the station accepts and is silent */
    void               *ctx;      /**< the station's own state */
};

/** @brief The octobus fabric: one slot per 6-bit station number, plus routing. */
typedef struct NdbusFabric
{
    NdbusStation      *stations[NDBUS_STATION_SLOTS]; /**< slot per station number; NULL = absent */
    int                station_count;                 /**< how many slots are occupied */
    const NdbusHostOps *host;     /**< host callbacks for logging; may be NULL */
} NdbusFabric;

/**
 * @brief Bring a fabric up with no stations registered.
 * @param fabric The fabric to initialise.
 * @param host   Host callbacks for logging; may be NULL.
 * @return Nothing. Initialisation cannot fail.
 */
void ndbus_fabric_init(NdbusFabric *fabric, const NdbusHostOps *host);

/**
 * @brief Register a station on the fabric.
 * @param fabric  The fabric.
 * @param station The station, whose `number` selects the slot.
 * @return true when registered. false (no state changed) for station 0, anything
 *         above 76B, a NULL station, and a slot that is already occupied - the
 *         caller unregisters first.
 */
bool ndbus_fabric_register(NdbusFabric *fabric, NdbusStation *station);

/**
 * @brief Remove the station at `number`.
 * @param fabric The fabric.
 * @param number The station number to clear.
 * @return true when a station was removed, false when there was none.
 */
bool ndbus_fabric_unregister(NdbusFabric *fabric, uint8_t number);

/**
 * @brief Whether a station is registered at `number`.
 * @param fabric The fabric.
 * @param number The station number to test.
 * @return true when a station is registered there, false otherwise (including
 *         for an illegal station number).
 */
bool          ndbus_fabric_has_station(const NdbusFabric *fabric, uint8_t number);

/**
 * @brief Look up the station registered at `number`.
 * @param fabric The fabric.
 * @param number The station number to look up.
 * @return The station, or NULL when the slot is empty or the number is illegal.
 */
NdbusStation *ndbus_fabric_get_station(const NdbusFabric *fabric, uint8_t number);

/**
 * @brief How many stations are registered.
 * @param fabric The fabric.
 * @return The number of occupied slots, 0 when none. No error return.
 */
int           ndbus_fabric_station_count(const NdbusFabric *fabric);

/**
 * @brief The MASTER station.
 *
 * THE MASTER is the registered station with the LOWEST number (T330).
 *
 * @param fabric The fabric.
 * @return The lowest registered station number, or 0 when no station is
 *         registered - which is the "no MASTER selected, XRFO not pulsing"
 *         state, not an error.
 */
uint8_t ndbus_fabric_master(const NdbusFabric *fabric);

/**
 * @brief Send one frame across the fabric and collect the replies.
 *
 * Send one frame. The frame carries the DESTINATION in bits 13-8; the fabric
 * rewrites that field to `source_station` before delivery.
 *
 * Broadcast (B=1) is delivered to every registered station except the sender,
 * and their replies are concatenated. EMULATOR SIMPLIFICATION, carried over from
 * RetroCore: the real bus matches the destination field against a 3-bit
 * broadcast TYPE in the BT register, and the type-code table is not documented
 * in anything available - the DOMINO manual defers to an "OCTObus Protocol
 * Specification" that has not been found. Correct it here if that table surfaces.
 *
 * @param fabric         The fabric.
 * @param source_station The sending station number, written into bits 13-8 on
 *                       delivery.
 * @param frame          The frame in SEND format, destination in bits 13-8.
 * @param replies        Buffer that must hold at least NDBUS_MAX_REPLY_FRAMES
 *                       frames.
 * @return The number of reply frames written to `replies`, or -1 for a TIMEOUT -
 *         an illegal destination or no station there. Timeout is what the real
 *         bus reports as Ack=00, and it is deliberately distinct from 0
 *         ("delivered, no reply"): a station that is absent must not look like a
 *         station that is quiet. A caller that tests only for 0 will read an
 *         empty slot as a silent one.
 */
int ndbus_fabric_send(NdbusFabric *fabric, uint8_t source_station, uint16_t frame,
                      uint16_t *replies);

/* ---- arbitration priority ---------------------------------------------------
 *
 * A station that loses arbitration has its priority INCREMENTED (T330), so a
 * station that keeps losing eventually wins and nothing starves. The fabric
 * above delivers one frame at a time and never has two senders at once, so it
 * does not arbitrate; this counter is the piece of the protocol that a station
 * implementation needs, kept here so there is one definition of it.
 */

/** @brief One station's arbitration priority counter. */
typedef struct NdbusArbiter
{
    uint8_t priority; /**< 0..NDBUS_PRIORITY_MAX; incremented on every give-up */
} NdbusArbiter;

/** @brief The priority field in the frame is 4 bits wide (T331). */
#define NDBUS_PRIORITY_MAX 15u

/**
 * @brief Set a station's arbitration priority back to 0.
 * @param arbiter The arbiter to reset.
 * @return Nothing. Cannot fail.
 */
void ndbus_arbiter_reset(NdbusArbiter *arbiter);

/**
 * @brief This station gave up: increment its priority.
 * @param arbiter The arbiter of the station that lost.
 * @return The new priority, saturating at NDBUS_PRIORITY_MAX. Saturation is not
 *         reported: there is no error return, because the frame field is only
 *         four bits wide and a higher value could not be transmitted anyway.
 */
uint8_t ndbus_arbiter_lost(NdbusArbiter *arbiter);

/**
 * @brief This station won and sent its frame: priority returns to 0.
 * @param arbiter The arbiter of the station that won.
 * @return Nothing. Cannot fail.
 */
void ndbus_arbiter_won(NdbusArbiter *arbiter);

/**
 * @brief Which of two requesting stations wins.
 *
 * INFERRED, not quoted. What T330 states is the give-up rule: "Each requesting
 * station goes on transmitting until it receives a '1' while transmitting a '0'
 * itself. Then it ceases transmitting ... At the time a station gives up, its
 * priority is incremented." On a wire-ORed serial line that means the larger
 * transmitted value survives, so a HIGHER priority wins and, with priorities
 * equal, the LOWER station number wins - the same rule that makes the lowest
 * station the MASTER. Nothing available states the tie-break outright, and no
 * instruction the guest can execute observes it (arbitration order is in the
 * "deliberately not modelled" list of architecture document section 7.5.8), so
 * this exists to keep a station implementation honest rather than to reproduce
 * bus timing.
 *
 * @param station_a  First requesting station number.
 * @param priority_a First station's arbitration priority.
 * @param station_b  Second requesting station number.
 * @param priority_b Second station's arbitration priority.
 * @return The station number of the winner. There is no error return: both
 *         inputs are station numbers and one of them always wins.
 */
uint8_t ndbus_arbitrate(uint8_t station_a, uint8_t priority_a, uint8_t station_b,
                        uint8_t priority_b);

#endif /* NDBUS_OCTOBUS_H */
