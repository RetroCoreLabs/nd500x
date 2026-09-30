/**
 * @file ndbus_multibyte.h
 * @brief Reassemble one octobus multibyte message.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Every octobus station that consumes multibyte messages needs the same
 * accumulation mechanics:
 *
 *   SOMB   (C=1, M=1, S=1)  opens the message and records the sender
 *   data   (C=0)            each frame contributes ONE body byte
 *   EOMB   (C=1, M=1, S=0)  closes it and hands the body to the consumer
 *
 * ND-05.020.01 T124, figure 28 "Octobus Multibyte Message Format": the body is
 * source OMD, body length, then the message body, and "the message frame is
 * added/removed by the octobus driver" - which is this file.
 *
 * Only the frame ROUTING differs per station (which OMD numbers it serves,
 * whether other OMDs go to a microprogram, idle gating), so the routing stays in
 * each station and this is the one shared implementation of the accumulation.
 * Composed, not inherited: a station owns one collector per independent message
 * stream it reassembles.
 */

#ifndef NDBUS_MULTIBYTE_H
#define NDBUS_MULTIBYTE_H

#include "ndbus_octobus.h"
#include "ndbus_types.h"

/** @brief A multibyte body is at most 255 bytes: the length is one byte on the wire. */
#define NDBUS_MULTIBYTE_MAX 255

/**
 * @brief State of one multibyte message being reassembled from octobus frames.
 */
typedef struct NdbusMultibyte
{
    uint8_t bytes[NDBUS_MULTIBYTE_MAX];  /**< body bytes collected so far */
    int     count;     /**< how many body bytes are in `bytes` */
    bool    open;      /**< a SOMB was seen and the matching EOMB has not */
    bool    overflow;  /**< more body bytes arrived than the wire can describe */
    uint8_t source;    /**< who sent the message being collected; replies go here */
} NdbusMultibyte;

/**
 * @brief Clear a collector back to idle: nothing open, nothing collected.
 * @param mb The collector to reset.
 * @return Nothing.
 */
void ndbus_multibyte_reset(NdbusMultibyte *mb);

/**
 * @brief SOMB seen: start a fresh message and remember the sender.
 * @param mb     The collector.
 * @param source The sending OMD number, kept so replies can be addressed back.
 * @return Nothing.
 * @note Any half-collected previous message is DISCARDED - a new SOMB always
 *       wins, because the sender having restarted is the only way a second SOMB
 *       can arrive.
 */
void ndbus_multibyte_begin(NdbusMultibyte *mb, uint8_t source);

/**
 * @brief One body byte from a data frame.
 * @param mb    The collector.
 * @param value The body byte carried by the frame.
 * @return true when the byte was stored; false when the collector is not open
 *         (a stray data frame, which is dropped) or the body is already full.
 */
bool ndbus_multibyte_push(NdbusMultibyte *mb, uint8_t value);

/**
 * @brief EOMB seen: close the message.
 * @param mb The collector.
 * @return true when a complete body is now ready for the consumer; false when
 *         nothing was open, or when the body overflowed - an overflowed message
 *         is refused whole rather than delivered truncated, because a truncated
 *         ACCP command is a DIFFERENT command.
 */
bool ndbus_multibyte_end(NdbusMultibyte *mb);

/**
 * @brief Build one COMPLETE multibyte message as octobus reply frames.
 *
 * The envelope is part of the message, and leaving any of it off produces a
 * DIFFERENT message rather than a shorter one:
 *
 *     SOMB     C=1, station, low byte = M|S|OMD  (0x30 | OMD)
 *     data     C=0, station, low byte = OUR source OMD - where the receiver replies
 *     data     C=0, station, low byte = payload byte count N
 *     data x N C=0, station, low byte = payload byte
 *     EOMB     C=1, station, low byte = M|OMD    (0x20 | OMD)
 *
 * Ported from RetroCore NDBusOctobus.cs SendMultibyteMessage, whose envelope is
 * byte-verified against the TPE OCTOBUS B00 sender
 * (octobus_send_multibyte_message @ ram:d16a - SOMB built at ram:d1ae
 * "SAA 30B ; ORA OMD", EOMB at ram:d1f1 "SAA 20B ; ORA OMD").
 *
 * THE STATION FIELD IS THE SENDER'S AND IS STAMPED HERE. On the outbound path
 * the fabric rewrites bits 13-8 from destination to source, but a reply travels
 * back through the replies[] array and nothing rewrites it, so a reply built
 * with a zero station arrives claiming to come from station 0 - which is not a
 * legal station at all. The receiver reads those bits to know who answered
 * (TPE octobus_decode_frame_word @ ram:d3ae, mask 0x3F00 at ram:d3cb).
 *
 * @param station       The ANSWERING station number, stamped into every frame.
 * @param dest_omd      The OMD at the receiver this message is addressed to.
 * @param source_omd    Our OMD: where the receiver sends anything back.
 * @param payload       The payload bytes; may be NULL when payload_count is 0.
 * @param payload_count How many payload bytes, 0..255.
 * @param replies       Buffer for the frames.
 * @param max           Room in `replies`, in frames.
 * @return The number of frames written: 4 + payload_count. 0 when the whole
 *         message does not fit, or the count is out of range - NEVER a partial
 *         message, because half an envelope is a different message.
 */
int ndbus_multibyte_build(uint8_t station, uint8_t dest_omd, uint8_t source_omd,
                          const uint8_t *payload, int payload_count, uint16_t *replies, int max);

#endif /* NDBUS_MULTIBYTE_H */
