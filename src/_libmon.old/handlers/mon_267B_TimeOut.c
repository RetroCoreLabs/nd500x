/*
 * MON 267B (183 decimal): TimeOut (TMOUT)
 *
 * Suspends the execution of your program for a given time. The execution then continues after the monitor call. The restart cause is indicated. Avoid using Time Out from ND-500, use ND500TimeOut instead.
 * 
 * - No reserved files or devices are released.
 * - The execution continues immediately if the program has its restart flag set.
 * - You may use NoWaitSwitch. Then the program restarts when a break occurs.
 * - If a program has been rescheduled by the monitor call SET (mon 101, DelayStart) or ABSET (mon 102, StartupTime), this rescheduling will be destroyed when you use TimeOut.
 *
 * Parameters:
 *   [I] NoTimeUnits (INTEGER): input
 *   [I] UnitType (INTEGER): input
 *   [O] RestartReason (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_267B_TimeOut(MonContext* ctx) {
    /* TODO: Implement TimeOut (TMOUT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "NoTimeUnits");
    MON_LOG_IN_WORD(ctx, 1, "UnitType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
