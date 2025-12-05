/*
 * MON 103B (67 decimal): StartupInterval (INTV)
 *
 * Prepares an RT program for periodic execution. The interval between the executions can be specified in hours, minutes, seconds, and basic time units. A basic time unit is 1/50th of a second.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER): input
 *   [I] Time (INTEGER): input
 *   [I] Units (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_103B_StartupInterval(MonContext* ctx) {
    /* TODO: Implement StartupInterval (INTV) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");
    MON_LOG_IN_WORD(ctx, 1, "Time");
    MON_LOG_IN_WORD(ctx, 2, "Units");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
