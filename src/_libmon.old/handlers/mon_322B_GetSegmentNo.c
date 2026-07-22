/*
 * MON 322B (210 decimal): GetSegmentNo (GSGNO)
 *
 * Gets the number of a segment in the ND-100. You specify the segment name. Segment names are created with the RT LOADER or when a program is dumped reentrant. See @DUMP-PROGRAM-REENTRANT.
 *
 * Parameters:
 *   [I] SegmentName (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_322B_GetSegmentNo(MonContext* ctx) {
    /* TODO: Implement GetSegmentNo (GSGNO) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "SegmentName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
