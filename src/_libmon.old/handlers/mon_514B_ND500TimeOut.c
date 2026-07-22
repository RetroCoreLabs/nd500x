/*
 * MON 514B (332 decimal): ND500TimeOut (5TMOUT)
 *
 * Suspends the execution of an ND-500 program for a given time. The execution then continues after the monitor call. The program is placed in a time queue in the ND-500, not the ND-100.
 * 
 * - No reserved files or devices are released.
 * - Avoid using TimeOut (mon 267) from ND-500, use ND500TimeOut instead. Use TimeOut from ND-100.
 *
 * Parameters:
 *   [I] NoOfTimeUnits (INTEGER): input
 *   [I] TimeUnit (INTEGER): input
 *   [O] ReturnStatus (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_514B_ND500TimeOut(MonContext* ctx) {
    /* TODO: Implement ND500TimeOut (5TMOUT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "NoOfTimeUnits");
    MON_LOG_IN_WORD(ctx, 1, "TimeUnit");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
