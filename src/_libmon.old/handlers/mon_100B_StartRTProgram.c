/*
 * MON 100B (64 decimal): StartRTProgram (RT)
 *
 * Starts an RT program. The program is moved to the execution queue. It is executed according to its priority.
 * 
 * - The program may be in the execution queue. Then it restarts as soon as it terminates.
 * - You can terminate RT programs with StopRTProgram.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_100B_StartRTProgram(MonContext* ctx) {
    /* TODO: Implement StartRTProgram (RT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
