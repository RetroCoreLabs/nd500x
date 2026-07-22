/*
 * MON 167B (119 decimal): AttachSegment (REENT)
 *
 * Attaches a reentrant segment to your two current segments. The address areas of the segments may overlap. Pages in the reentrant segment may be accessed for reading, writing, or fetching instructions. When written to, a page loses its reentrancy. It is stored on one of your current overlapping segments.
 *
 * Parameters:
 *   [I] SegmentNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_167B_AttachSegment(MonContext* ctx) {
    /* TODO: Implement AttachSegment (REENT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "SegmentNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
