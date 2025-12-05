/*
 * MON 500B (320 decimal): StartProcess (STARTP)
 *
 * Starts a process in the ND-500. You identify the process with the process number.
 *
 * Parameters:
 *   [I] ProcessNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_500B_StartProcess(MonContext* ctx) {
    /* TODO: Implement StartProcess (STARTP) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "ProcessNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
