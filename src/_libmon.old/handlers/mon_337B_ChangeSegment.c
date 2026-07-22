/*
 * MON 337B (223 decimal): ChangeSegment (SPCHG)
 *
 * Changes the segment and the page table your program uses. The monitor call is similar to JumpToSegment and ExitFromSegment. In addition, you may change the two page tables in use. The segment numbers are restricted to 8-bits (values 0-255). SegmentFunction (MON 341) is the equivalent monitor call for version K of SINTRAN III.
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_337B_ChangeSegment(MonContext* ctx) {
    /* TODO: Implement ChangeSegment (SPCHG) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
