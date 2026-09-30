/*
 * ndbus_multibyte.c - octobus multibyte message reassembly
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include <string.h>

#include "ndbus_multibyte.h"

void ndbus_multibyte_reset(NdbusMultibyte *mb)
{
    if (mb == NULL)
    {
        return;
    }
    memset(mb, 0, sizeof(*mb));
}

void ndbus_multibyte_begin(NdbusMultibyte *mb, uint8_t source)
{
    if (mb == NULL)
    {
        return;
    }
    mb->count    = 0;
    mb->open     = true;
    mb->overflow = false;
    mb->source   = source;
}

bool ndbus_multibyte_push(NdbusMultibyte *mb, uint8_t value)
{
    if (mb == NULL || !mb->open)
    {
        return false; /* a stray data frame outside a message */
    }
    if (mb->count >= NDBUS_MULTIBYTE_MAX)
    {
        mb->overflow = true;
        return false;
    }
    mb->bytes[mb->count++] = value;
    return true;
}

bool ndbus_multibyte_end(NdbusMultibyte *mb)
{
    if (mb == NULL || !mb->open)
    {
        return false;
    }
    mb->open = false;
    /* An overflowed body is refused whole. A truncated ACCP command is a
     * different command, and delivering one would act on it. */
    return !mb->overflow;
}

int ndbus_multibyte_build(uint8_t station, uint8_t dest_omd, uint8_t source_omd,
                          const uint8_t *payload, int payload_count, uint16_t *replies, int max)
{
    if (replies == NULL || payload_count < 0 || payload_count > NDBUS_MULTIBYTE_MAX)
    {
        return 0;
    }
    if (payload == NULL && payload_count > 0)
    {
        return 0;
    }

    const int frames = 4 + payload_count; /* SOMB + srcOMD + count + payload + EOMB */
    if (frames > max)
    {
        return 0;
    }

    const uint16_t station_bits =
        (uint16_t)(((uint16_t)station & 0x3Fu) << NDBUS_FRAME_STATION_SHIFT);
    const uint16_t omd = (uint16_t)(dest_omd & 0x0Fu);

    int n = 0;
    replies[n++] = (uint16_t)(NDBUS_FRAME_C_CONTROL | station_bits | NDBUS_FRAME_M_MULTIBYTE |
                              NDBUS_FRAME_S_STARTSTOP | omd);
    replies[n++] = (uint16_t)(station_bits | (uint16_t)source_omd);
    replies[n++] = (uint16_t)(station_bits | (uint16_t)payload_count);
    for (int i = 0; i < payload_count; i++)
    {
        replies[n++] = (uint16_t)(station_bits | (uint16_t)payload[i]);
    }
    replies[n++] = (uint16_t)(NDBUS_FRAME_C_CONTROL | station_bits | NDBUS_FRAME_M_MULTIBYTE | omd);
    return n;
}
