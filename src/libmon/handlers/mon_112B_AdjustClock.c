/*
 * MON 112B (74 decimal): AdjustClock (CLADJ)
 *
 * Sets the computer's clock (i.e. the current system time) forward or back. If the operator panel has a clock, it is also adjusted.
 * 
 * - The startup time for RT programs can be set by StartupTime. When the current system time is modified, the next startup times of periodic programs started by StartupTime are also affected. Other scheduling times are not affected.
 *
 * Parameters:
 *   [I] TimeUnits (INTEGER2): input
 *   [I] UnitType (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_112B_AdjustClock(MonContext* ctx) {
    /* TODO: Implement AdjustClock (CLADJ) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "TimeUnits");
    MON_LOG_IN_WORD(ctx, 1, "UnitType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
