/*
 * MON 105B (69 decimal): StopRTProgram (ABORT)
 *
 * Stops an RT program. It is removed from the time or execution queue. All reserved devices and files are released. RT programs schedules for periodic execution are also stopped.
 * 
 * - Nothing happens if the RT program is already stopped.
 * - The RT program restarts immediately if its restart flag is set.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_105B_StopRTProgram(MonContext* ctx) {
    /* TODO: Implement StopRTProgram (ABORT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
