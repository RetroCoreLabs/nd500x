/*
 * MON 157B (111 decimal): SegmentToPageTable (ENTSG)
 *
 * Enters a routine as a direct task or as a device driver, and \"remembers\" which segments have been entered (up to 24 segments). These are reentered at restart following a power failure. The routines are connected to the interrupt system. They are loaded with the RT LOADER or by DMAC.
 * 
 * - The segment where the routine resides must be fixed in memory. See FixScattered.
 * - The segments can be removed from the selected PIT by using function 4 of ChangeActiveSegments (MON 341).
 * - Unfixing the segment (using UnFixSegment, MON 116), removes it from the Page Index Table.
 *
 * Parameters:
 *   [I] SegmentNo (INTEGER2): input
 *   [I] PageTable (INTEGER2): input
 *   [I] InterruptLevel (INTEGER2): input
 *   [I] StartAddress (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_157B_SegmentToPageTable(MonContext* ctx) {
    /* TODO: Implement SegmentToPageTable (ENTSG) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "SegmentNo");
    MON_LOG_IN_WORD(ctx, 1, "PageTable");
    MON_LOG_IN_WORD(ctx, 2, "InterruptLevel");
    MON_LOG_IN_WORD(ctx, 3, "StartAddress");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
