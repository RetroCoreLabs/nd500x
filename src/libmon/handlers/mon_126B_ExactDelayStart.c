/*
 * MON 126B (86 decimal): ExactDelayStart (DSET)
 *
 * Sets an RT program to start after a given period. It is then moved from the time queue to the execution queue. The period is specified in basic time units. A basic time unit is 1/50th of a second. The period may be from 1 to 4294967647 basic time units.
 * 
 * - The program may already be in the time queue. It is then reinserted according to the new specifications.
 * - A period less than or equal to 0 transfers the RT program to the execution queue the next time the basic time unit counter is incremented.
 * - SetClock, AdjustClock and @CLADJ do not affect the interval.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER): input
 *   [I] BasicTimeUnits (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_126B_ExactDelayStart(MonContext* ctx) {
    /* TODO: Implement ExactDelayStart (DSET) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");
    MON_LOG_IN_WORD(ctx, 1, "BasicTimeUnits");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
