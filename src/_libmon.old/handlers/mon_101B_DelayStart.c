/*
 * MON 101B (65 decimal): DelayStart (SET)
 *
 * Starts an RT program after a specified time. The RT program is put in the time queue. It is moved to the execution queue after the specified period.
 * 
 * - RT programs already in the time queue are reinserted according to the new specifications.
 * - AdjustClock and @CLADJ do not affect the specified period.
 * - A period less than or equal to 0 moves the RT program to the execution queue the next time the basic time unit counter is incremented.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER2): input
 *   [I] TimeUnits (INTEGER2): input
 *   [I] UnitType (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_101B_DelayStart(MonContext* ctx) {
    /* TODO: Implement DelayStart (SET) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");
    MON_LOG_IN_WORD(ctx, 1, "TimeUnits");
    MON_LOG_IN_WORD(ctx, 2, "UnitType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
