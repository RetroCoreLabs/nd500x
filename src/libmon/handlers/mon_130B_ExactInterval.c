/*
 * MON 130B (88 decimal): ExactInterval (DINTV)
 *
 * Prepares an RT program for periodic execution. The interval between the executions may be from 1 to 4294967647 basic time units. A basic time unit is 1/50th of a second.
 * 
 * - The RT program is not started. Use, for example, StartRTProgram or @RT to start it.
 * - StopRTProgram, Disconnect, @ABORT or @DSCNT cancel this monitor call.
 * - One execution may be unfinished when it is time for the next execution. In this case, the program's restart flag is set. If the delay becomes as long as two intervals, one execution is lost.
 * - The interval replaces any earlier specified intervals.
 * - AdjustClock and @CLADJ do not affect the interval.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER): input
 *   [I] BasicTimeUnits (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_130B_ExactInterval(MonContext* ctx) {
    /* TODO: Implement ExactInterval (DINTV) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");
    MON_LOG_IN_WORD(ctx, 1, "BasicTimeUnits");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
