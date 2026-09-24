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
