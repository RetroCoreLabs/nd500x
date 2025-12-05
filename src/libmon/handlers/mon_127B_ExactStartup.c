/*
 * MON 127B (87 decimal): ExactStartup (DABST)
 *
 * Starts an RT program at a specific time. The time is given in basic time units. A basic time unit is 1/50th of a second. The RT program is moved from the time queue to the execution queue at the specified time.
 * 
 * - It may already be later than the time specified. The RT program is then scheduled for the next day.
 * - The RT program may already be in the time queue. It is then reinserted according to the new time specifications.
 * - AdjustClock and @CLADJ affect the startup. The RT program starts according to the new time.
 * - Use GetBasicTime to read the internal time in basic time units.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER2): input
 *   [I] BasicTimeUnits (LONGINT): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_127B_ExactStartup(MonContext* ctx) {
    /* TODO: Implement ExactStartup (DABST) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");
    MON_LOG_IN_WORD(ctx, 1, "BasicTimeUnits");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
