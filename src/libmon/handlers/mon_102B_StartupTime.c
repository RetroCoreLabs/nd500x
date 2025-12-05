/*
 * MON 102B (66 decimal): StartupTime (ABSET)
 *
 * Starts an RT program at a specified time of the day. The RT program is then put in the time queue. It is moved to the execution queue at the specified time.
 * 
 * - The time of the day specified may have already passed. In this case, the program starts the next day.
 * - RT programs already in the time queue are reinserted according to the new specifications.
 * - AdjustClock, and @CLADJ affect the system's clock. Whenever the system time is changed, RT programs start according to the new time.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER): input
 *   [I] Seconds (INTEGER): input
 *   [I] Minutes (INTEGER): input
 *   [I] Hours (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_102B_StartupTime(MonContext* ctx) {
    /* TODO: Implement StartupTime (ABSET) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");
    MON_LOG_IN_WORD(ctx, 1, "Seconds");
    MON_LOG_IN_WORD(ctx, 2, "Minutes");
    MON_LOG_IN_WORD(ctx, 3, "Hours");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
