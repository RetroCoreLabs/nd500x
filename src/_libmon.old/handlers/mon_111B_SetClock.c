/*
 * MON 111B (73 decimal): SetClock (UPDAT)
 *
 * Gives new values to the computer's clock and calendar. If the computer panel has a clock, it is updated.
 * 
 * - The startup time for RT programs can be set by StartupTime. Such RT programs start according to the new time.
 * - Illegal time values, e.g. 61 minutes, stop the program and output a message.
 *
 * Parameters:
 *   [I] Minute (INTEGER): input
 *   [I] Hour (INTEGER): input
 *   [I] Day (INTEGER): input
 *   [I] Month (INTEGER): input
 *   [I] Year (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_111B_SetClock(MonContext* ctx) {
    /* TODO: Implement SetClock (UPDAT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Minute");
    MON_LOG_IN_WORD(ctx, 1, "Hour");
    MON_LOG_IN_WORD(ctx, 2, "Day");
    MON_LOG_IN_WORD(ctx, 3, "Month");
    MON_LOG_IN_WORD(ctx, 4, "Year");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
