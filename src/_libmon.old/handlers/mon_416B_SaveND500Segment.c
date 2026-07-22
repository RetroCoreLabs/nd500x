/*
 * MON 416B (270 decimal): SaveND500Segment (WSEGN)
 *
 * Writes all modified pages of a segment back to the disk.
 * 
 * - Not allowed when fixed in memory.
 *
 * Parameters:
 *   [I] LogSegmentNo (INTEGER2): input
 *   [I] FirstPage (INTEGER2): input
 *   [I] LastPage (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_416B_SaveND500Segment(MonContext* ctx) {
    /* TODO: Implement SaveND500Segment (WSEGN) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "LogSegmentNo");
    MON_LOG_IN_WORD(ctx, 1, "FirstPage");
    MON_LOG_IN_WORD(ctx, 2, "LastPage");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
