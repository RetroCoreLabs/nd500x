/*
 * MON 115B (77 decimal): FixScattered (FIX)
 *
 * Place a segment in physical memory. Its pages will no longer be swapped to the disk. The segment must be non demand. Its pages will be scattered in physical memory. You may, for example, use this function for time critical operations or for allocating DMA buffers.
 * 
 * - Use UnFixSegment or @UNFIX to allow the RT LOADER to clear the segment.
 * - Your program terminates if you refer to a nonexistent segment or a demand segment. An error message is output on the error device.
 * - Only a limited number of pages can be fixed. This limit is defined when SINTRAN III is generated. You may change it with the command CHANGE-VARIABLE in the SINTRAN-SERVICE-PROGRAM.
 *
 * Parameters:
 *   [I] SegmentNumber (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_115B_FixScattered(MonContext* ctx) {
    /* TODO: Implement FixScattered (FIX) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "SegmentNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
