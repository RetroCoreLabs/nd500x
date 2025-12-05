/*
 * MON 116B (78 decimal): UnfixSegment (UNFIX)
 *
 * Releases a fixed segment and removes it from the Page Index Table (PIT). Its pages may then be swapped from physical memory to the disk. Use FixScattered or FixContiguous to fix the segment.
 * 
 * - You must use UnFixSegment or @UNFIX to allow the RT-LOADER to clear the segment.
 *
 * Parameters:
 *   [I] SegmentNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_116B_UnfixSegment(MonContext* ctx) {
    /* TODO: Implement UnfixSegment (UNFIX) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "SegmentNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
