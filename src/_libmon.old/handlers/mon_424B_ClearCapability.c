/*
 * MON 424B (276 decimal): ClearCapability (CAPCLE)
 *
 * Clears a capability. A capability describes each logical segment in a domain. The protection of the segment is removed. See the manual ND Linker User Guide and Reference Manual (ND-60289).
 * 
 * - The logical segment is available for other physical segments.
 *
 * Parameters:
 *   [I] LogicalSegmentNo (INTEGER2): input
 *   [I] SegmentType (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_424B_ClearCapability(MonContext* ctx) {
    /* TODO: Implement ClearCapability (CAPCLE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "LogicalSegmentNo");
    MON_LOG_IN_WORD(ctx, 1, "SegmentType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
