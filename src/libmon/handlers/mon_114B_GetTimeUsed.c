/*
 * MON 114B (76 decimal): GetTimeUsed (TUSED)
 *
 * Gets the time you have used the CPU since you logged in. In batch jobs, you get the time since you entered the job.
 * 
 * - The CPU time used is given in basic time units. A basic time unit is 1/50th of a second.
 * - Can also be used from RT-programs.
 *
 * Parameters:
 *   [O] TimeUsed (LONGINT): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_114B_GetTimeUsed(MonContext* ctx) {
    /* TODO: Implement GetTimeUsed (TUSED) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
