/*
 * MON 133B (91 decimal): ExitFromSegment (MEXIT)
 *
 * Exchanges one or both current segments. Commonly used to return after the monitor call JumpToSegment. ChangeSegment may be used instead of JumpToSegment and ExitFromSegment. The segment numbers are restricted to 8-bits (values 0-255). Use SegmentFunction (MON 341) with version K of SINTRAN III.
 *
 * Parameters:
 *   [I] SegmentNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_133B_ExitFromSegment(MonContext* ctx) {
    /* TODO: Implement ExitFromSegment (MEXIT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "SegmentNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
