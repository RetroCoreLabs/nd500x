/*
 * MON 426B (278 decimal): GetProcessNo (GPRNAM)
 *
 * Gets the number of a process in the ND-500. You specify the process name. The process number is assigned when you start the ND-500 Monitor.
 * 
 * - The process name may not be abbreviated.
 *
 * Parameters:
 *   [I] ProcessName (STRING): input
 *   [O] ProcessNumber (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_426B_GetProcessNo(MonContext* ctx) {
    /* TODO: Implement GetProcessNo (GPRNAM) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "ProcessName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
