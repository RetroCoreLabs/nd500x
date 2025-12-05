/*
 * MON 134B (92 decimal): ExitRTProgram (RTEXT)
 *
 * Terminates the calling RT or background program. Releases all reserved resources. The monitor call has the same effect as exit for interactive background programs.
 * 
 * - Batch jobs are aborted.
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_134B_ExitRTProgram(MonContext* ctx) {
    /* TODO: Implement ExitRTProgram (RTEXT) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
