/*
 * ndbus_multibyte.h - reassemble one octobus multibyte message
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

#include "ndbus_types.h"

/* A multibyte body is at most 255 bytes: the length is one byte on the wire. */
#define NDBUS_MULTIBYTE_MAX 255

typedef struct NdbusMultibyte
{
    uint8_t bytes[NDBUS_MULTIBYTE_MAX];
    int     count;
    bool    open;      /* a SOMB was seen and the matching EOMB has not */
    bool    overflow;  /* more body bytes arrived than the wire can describe */
    uint8_t source;    /* who sent the message being collected; replies go here */
} NdbusMultibyte;

void ndbus_multibyte_reset(NdbusMultibyte *mb);

/* SOMB seen: start a fresh message and remember the sender. Any half-collected
 * previous message is DISCARDED - a new SOMB always wins, because the sender
 * having restarted is the only way a second SOMB can arrive. */
void ndbus_multibyte_begin(NdbusMultibyte *mb, uint8_t source);

/* One body byte from a data frame. Returns false when the collector is not open
 * (a stray data frame, which is dropped) or the body is already full. */
bool ndbus_multibyte_push(NdbusMultibyte *mb, uint8_t value);

/* EOMB seen: close the message. Returns false when nothing was open, or when the
 * body overflowed - an overflowed message is refused whole rather than delivered
 * truncated, because a truncated ACCP command is a DIFFERENT command. */
bool ndbus_multibyte_end(NdbusMultibyte *mb);

#endif /* NDBUS_MULTIBYTE_H */
