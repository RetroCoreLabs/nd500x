/*
 * MON 104B (68 decimal): SuspendProgram (HOLD)
 *
 * Suspends the execution of your program for a given time. The execution then continues after the time specified by the monitor call.
 *
 * Parameters:
 *   [I] TimeUnits (INTEGER): input
 *   [I] UnitType (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_104B_SuspendProgram(MonContext* ctx) {
    /* TODO: Implement SuspendProgram (HOLD) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "TimeUnits");
    MON_LOG_IN_WORD(ctx, 1, "UnitType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
