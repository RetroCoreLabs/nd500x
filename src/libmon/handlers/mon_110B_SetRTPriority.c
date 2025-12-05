/*
 * MON 110B (72 decimal): SetRTPriority (PRIOR)
 *
 * Sets the priority of an RT program. RT programs may be given priorities from 0 to 255. SINTRAN III executes the RT program with the highest priority.
 * 
 * - The priority of background programs vary between 20 and 60.
 * - Programs with priority 0 will never start. You may use this to suspend programs.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER): input
 *   [I] PriorityLevel (INTEGER): input
 *   [O] OldPriority (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_110B_SetRTPriority(MonContext* ctx) {
    /* TODO: Implement SetRTPriority (PRIOR) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");
    MON_LOG_IN_WORD(ctx, 1, "PriorityLevel");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
