/**
 * @file ndbus_cpunum.h
 * @brief The two CPU numberings, converted in one place.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * An ND-5000 has three numbers and they are not the same number:
 *
 *   STATION   070B..076B (56..62 decimal) - its address on the octobus
 *   CPUNO     1..7                        - its mailbox extension block
 *   X5CPU     0..6                        - its context block
 *
 * All three are what their own sources say. The two block numberings are the
 * dangerous pair: both index a 256-byte stride off a base, so the expressions
 * look interchangeable, and passing one where the other belongs puts a CPU's
 * registers in its neighbour's block or its queue head on top of the mailbox
 * global header. Neither faults. They corrupt.
 *
 * So the conversion lives here and nowhere else. A bare
 * `station - NDBUS_STATION_ND5000_FIRST` in calling code is the bug this file
 * exists to prevent.
 */

#ifndef NDBUS_CPUNUM_H
#define NDBUS_CPUNUM_H

#include "ndbus_octobus.h"

/**
 * @brief Mailbox CPUNO for an octobus station. ONE-BASED.
 * @param station Octobus station, 070B..076B.
 * @return CPUNO 1..7, or 0 for a station outside the ND-5000 range - which is
 *         not a legal CPUNO, so a caller that ignores the result gets a refusal
 *         from ndbus_mailbox_attach() rather than a wrong block.
 */
static inline int ndbus_cpu_mailbox_cpuno(uint8_t station)
{
    if (station < NDBUS_STATION_ND5000_FIRST || station > NDBUS_STATION_ND5000_LAST)
    {
        return 0;
    }
    return (int)(station - NDBUS_STATION_ND5000_FIRST) + 1;
}

/**
 * @brief Context block X5CPU for an octobus station. ZERO-BASED.
 * @param station Octobus station, 070B..076B.
 * @return X5CPU 0..6, or -1 for a station outside the ND-5000 range - which
 *         ndbus_context_attach() refuses, rather than silently using block 0.
 */
static inline int ndbus_cpu_context_x5cpu(uint8_t station)
{
    if (station < NDBUS_STATION_ND5000_FIRST || station > NDBUS_STATION_ND5000_LAST)
    {
        return -1;
    }
    return (int)(station - NDBUS_STATION_ND5000_FIRST);
}

#endif /* NDBUS_CPUNUM_H */
