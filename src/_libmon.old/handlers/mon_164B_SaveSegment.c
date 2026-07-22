/*
 * MON 164B (116 decimal): SaveSegment (WSEG)
 *
 * Saves a segment in the ND-100. All pages in physical memory which have been changed, are written back to the disk.
 *
 * Parameters:
 *   [I] SegmentNumber (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_164B_SaveSegment(MonContext* ctx) {
    /* TODO: Implement SaveSegment (WSEG) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "SegmentNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
