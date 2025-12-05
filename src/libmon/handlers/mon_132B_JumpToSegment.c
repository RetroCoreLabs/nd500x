/*
 * MON 132B (90 decimal): JumpToSegment (MCALL)
 *
 * Calls a routine on another segment in the ND-100. You can divide an ND-100 RT program between various segments. This monitor call switches one or both of the current segments according to what you specify in parameter 2. ChangeSegment may be used instead of JumpToSegment and ExitFromSegment. The segment numbers are restricted to 8-bits (values 0-255). Use SegmentFunction (MON 341) with version K of SINTRAN III.
 *
 * Parameters:
 *   [I] SubroutineAddr (INTEGER): input
 *   [I] NewSegment (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_132B_JumpToSegment(MonContext* ctx) {
    /* TODO: Implement JumpToSegment (MCALL) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "SubroutineAddr");
    MON_LOG_IN_WORD(ctx, 1, "NewSegment");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
