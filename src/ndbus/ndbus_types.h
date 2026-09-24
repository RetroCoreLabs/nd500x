/*
 * ndbus_types.h - the two vtables through which ndbus reaches the two machines
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * THE RULE FOR EVERY FILE UNDER src/ndbus/:
 *
 *   Nothing here may include an emulator type. No Nd500Machine, no Nd500Cpu, no
 *   ND-100 header, no cpu_types.h. This directory models the MFbus (the MPM-5
 *   shared memory plus the octobus) and it reaches both machines ONLY through
 *   the two vtables below.
 *
 * The reason is testability: the coupling this code models depends on BOTH
 * machines by construction. Putting it in nd100x would make the ND-5000 half
 * untestable without a whole ND-100; giving it direct nd500x includes would
 * invert the dependency. With the vtables, layers 1, 3 and 4 of the test plan
 * run with two mock CPUs and no emulator linked at all.
 *
 * tools/check_ndbus_isolation.sh enforces the rule at build time. When this
 * directory is lifted into its own repository (the ndmonlib precedent), the rule
 * is what makes that a move rather than a rewrite.
 */

#ifndef NDBUS_TYPES_H
#define NDBUS_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The host: whoever owns the process. Threads, logging and wall time live here,
 * because ndbus must run natively (real threads) and under Emscripten (none).
 * Every field may be NULL; ndbus checks before calling.
 */
typedef struct NdbusHostOps
{
    /* Diagnostic text. level follows the emulator's own log levels; ndbus only
     * ever passes a preformatted, NUL-terminated ASCII string. */
    void (*log)(void *ctx, int level, const char *message);

    /* Yield the host CPU. Used ONLY by the TSET spin-damper (section 7.5.4):
     * a guest busy-waiting on a semaphore has no hardware analogue and would
     * otherwise burn a whole host core. NULL is legal - the damper then simply
     * does not yield. */
    void (*yield)(void *ctx);

    void *ctx;
} NdbusHostOps;

/*
 * A CPU attached to the bus - either the ND-100 or one ND-5000. The bus uses
 * this to deliver an interrupt or a doorbell and to ask what the CPU is.
 *
 * ADDRESSES IN THIS INTERFACE ARE POOL BYTE OFFSETS, not ND-100 word addresses
 * and not ND-5000 physical addresses. Each side converts at its own edge; see
 * section 7.2 of the architecture document.
 */
typedef struct NdbusCpuOps
{
    /* Raise the CPU's bus interrupt. `source_station` is the octal station that
     * caused it (1 for the ND-100 card, 070B..076B for an ND-5000). */
    void (*interrupt)(void *ctx, uint8_t source_station);

    /* The doorbell rang: a message is waiting at `pool_offset`. Called with the
     * acquire load already done, so the message bytes are visible. */
    void (*doorbell)(void *ctx, uint32_t pool_offset);

    /* Human-readable name for logs and test failures. Never NULL. */
    const char *name;

    void *ctx;
} NdbusCpuOps;

#endif /* NDBUS_TYPES_H */
