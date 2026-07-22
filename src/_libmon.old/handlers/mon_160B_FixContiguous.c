/*
 * MON 160B (112 decimal): FixContiguous (FIXC)
 *
 * Places a segment in physical memory. Its pages will no longer be swapped to the disk. The segment is placed in a contiguous area of physical memory. This function is useful for time-critical operations.
 * 
 * - The segment must be created with the RT LOADER as a non-demand segment.
 * - Use UnfixSegment or @UNFIX to allow the RT LOADER to clear the segment.
 * - Your program normally terminates if you refer to a nonexistent segment or a demand segment. An error message is output on the error device. See parameters 1 and 3 to get a status value instead.
 * - Only a limited number of pages can be fixed. This limit is defined when SINTRAN III is generated. You may change it with the command CHANGE-VARIABLE in the SINTRAN-SERVICE-PROGRAM.
 *
 * Parameters:
 *   [I] SegmentNo (INTEGER2): input
 *   [I] PageNumber (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_160B_FixContiguous(MonContext* ctx) {
    /* TODO: Implement FixContiguous (FIXC) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "SegmentNo");
    MON_LOG_IN_WORD(ctx, 1, "PageNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
