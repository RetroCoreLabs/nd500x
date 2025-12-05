/*
 * MON 507B (327 decimal): SetProcessPriority (SPRIO)
 *
 * Sets the priority for a process in the ND-500. The priorities vary from 0 to 255. The process with the highest priority is executed first.
 * 
 * - The priorities of background programs normally vary between 20 and 64. SINTRAN III modifies the priorities all the time. This is done to allow several jobs to share the CPU. Specify 0 to execute a process in the same way.
 * - With SetProcessPriority, you may fix the priority.
 *
 * Parameters:
 *   [I] NewPriority (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_507B_SetProcessPriority(MonContext* ctx) {
    /* TODO: Implement SetProcessPriority (SPRIO) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "NewPriority");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
