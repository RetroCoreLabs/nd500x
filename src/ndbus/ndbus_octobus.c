/*
 * ndbus_octobus.c - the octobus fabric
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include <stdio.h>
#include <string.h>

#include "ndbus_octobus.h"

static void fabric_log(const NdbusFabric *fabric, const char *message)
{
    if (fabric == NULL || fabric->host == NULL || fabric->host->log == NULL)
    {
        return;
    }
    fabric->host->log(fabric->host->ctx, 0, message);
}

void ndbus_fabric_init(NdbusFabric *fabric, const NdbusHostOps *host)
{
    if (fabric == NULL)
    {
        return;
    }
    memset(fabric, 0, sizeof(*fabric));
    fabric->host = host;
}

bool ndbus_fabric_register(NdbusFabric *fabric, NdbusStation *station)
{
    if (fabric == NULL || station == NULL)
    {
        return false;
    }

    uint8_t number = station->number;

    /* Station 0 and 77B are not in the T329 table. */
    if (number == 0 || number > NDBUS_STATION_MAX)
    {
        char text[96];
        (void)snprintf(text, sizeof(text), "octobus: register refused, illegal station %uB",
                       (unsigned)number);
        fabric_log(fabric, text);
        return false;
    }

    /* The caller unregisters before re-registering; a silent replace would hide
     * two owners fighting over one slot. */
    if (fabric->stations[number] != NULL)
    {
        char text[128];
        (void)snprintf(text, sizeof(text),
                       "octobus: register refused, station %u already held by %s", (unsigned)number,
                       fabric->stations[number]->type != NULL ? fabric->stations[number]->type
                                                              : "?");
        fabric_log(fabric, text);
        return false;
    }

    fabric->stations[number] = station;
    fabric->station_count++;
    return true;
}

bool ndbus_fabric_unregister(NdbusFabric *fabric, uint8_t number)
{
    if (fabric == NULL || number >= NDBUS_STATION_SLOTS)
    {
        return false;
    }
    if (fabric->stations[number] == NULL)
    {
        return false;
    }
    fabric->stations[number] = NULL;
    fabric->station_count--;
    return true;
}

bool ndbus_fabric_has_station(const NdbusFabric *fabric, uint8_t number)
{
    return ndbus_fabric_get_station(fabric, number) != NULL;
}

NdbusStation *ndbus_fabric_get_station(const NdbusFabric *fabric, uint8_t number)
{
    if (fabric == NULL || number >= NDBUS_STATION_SLOTS)
    {
        return NULL;
    }
    return fabric->stations[number];
}

int ndbus_fabric_station_count(const NdbusFabric *fabric)
{
    return (fabric != NULL) ? fabric->station_count : 0;
}

uint8_t ndbus_fabric_master(const NdbusFabric *fabric)
{
    if (fabric == NULL)
    {
        return 0;
    }
    /* "The one with the lowest station number ends up as the MASTER." */
    for (uint8_t i = 1; i <= NDBUS_STATION_MAX; i++)
    {
        if (fabric->stations[i] != NULL)
        {
            return i;
        }
    }
    return 0; /* no station: no MASTER, XRFO not pulsing */
}

/* Deliver one frame to one station and append its replies. Returns the number
 * appended; never more than the room left. */
static int deliver(NdbusStation *station, uint16_t delivery_frame, uint8_t source_station,
                   uint16_t *replies, int already)
{
    if (station->handle == NULL)
    {
        return 0; /* registered but silent */
    }

    uint16_t local[NDBUS_MAX_REPLY_FRAMES];
    int      count = station->handle(station, delivery_frame, source_station, local);
    if (count <= 0)
    {
        return 0;
    }
    if (count > NDBUS_MAX_REPLY_FRAMES)
    {
        count = NDBUS_MAX_REPLY_FRAMES;
    }

    int room = NDBUS_MAX_REPLY_FRAMES - already;
    if (count > room)
    {
        count = room;
    }
    for (int i = 0; i < count; i++)
    {
        replies[already + i] = local[i];
    }
    return count;
}

int ndbus_fabric_send(NdbusFabric *fabric, uint8_t source_station, uint16_t frame,
                      uint16_t *replies)
{
    if (fabric == NULL || replies == NULL)
    {
        return -1;
    }

    uint8_t destination = ndbus_frame_station(frame);
    bool    broadcast   = (frame & NDBUS_FRAME_B_BROADCAST) != 0;

    /* Bits 13-8 carry the destination on the way out and the source on the way
     * in. Rewrite them once, here, for both the unicast and broadcast paths. */
    uint16_t delivery_frame = ndbus_frame_set_station(frame, source_station);

    if (!broadcast)
    {
        if (destination == 0 || destination > NDBUS_STATION_MAX)
        {
            char text[96];
            (void)snprintf(text, sizeof(text), "octobus: illegal destination %u - timeout",
                           (unsigned)destination);
            fabric_log(fabric, text);
            return -1;
        }

        NdbusStation *station = fabric->stations[destination];
        if (station == NULL)
        {
            /* No station answers: the real bus reports Ack=00. Distinct from a
             * reply count of 0, so an absent station never looks like a quiet
             * one - the caller times out instead of waiting forever. */
            char text[96];
            (void)snprintf(text, sizeof(text), "octobus: no station at %u - timeout (Ack=00)",
                           (unsigned)destination);
            fabric_log(fabric, text);
            return -1;
        }

        return deliver(station, delivery_frame, source_station, replies, 0);
    }

    /* Broadcast: see the header - type-code matching is not modelled, so every
     * registered station except the sender receives the frame. */
    int total = 0;
    for (uint8_t i = 1; i <= NDBUS_STATION_MAX; i++)
    {
        if (i == source_station || fabric->stations[i] == NULL)
        {
            continue;
        }
        total += deliver(fabric->stations[i], delivery_frame, source_station, replies, total);
        if (total >= NDBUS_MAX_REPLY_FRAMES)
        {
            break;
        }
    }
    return total;
}

void ndbus_arbiter_reset(NdbusArbiter *arbiter)
{
    if (arbiter != NULL)
    {
        arbiter->priority = 0;
    }
}

uint8_t ndbus_arbiter_lost(NdbusArbiter *arbiter)
{
    if (arbiter == NULL)
    {
        return 0;
    }
    /* "At the time a station gives up, its priority is incremented." The field
     * is four bits wide, so it saturates rather than wrapping back to 0 - a
     * station that has lost fifteen times must not suddenly become the lowest
     * priority on the bus. */
    if (arbiter->priority < NDBUS_PRIORITY_MAX)
    {
        arbiter->priority++;
    }
    return arbiter->priority;
}

void ndbus_arbiter_won(NdbusArbiter *arbiter)
{
    if (arbiter != NULL)
    {
        arbiter->priority = 0;
    }
}

uint8_t ndbus_arbitrate(uint8_t station_a, uint8_t priority_a, uint8_t station_b,
                        uint8_t priority_b)
{
    if (priority_a != priority_b)
    {
        return (priority_a > priority_b) ? station_a : station_b;
    }
    return (station_a < station_b) ? station_a : station_b;
}
