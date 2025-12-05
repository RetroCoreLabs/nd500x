/*
 * MON 435B (285 decimal): ForceTrap (PRT)
 *
 * Forces a programmed trap to occur in another ND-500 process. The trap handler in this process is started.
 * 
 * - The trapped process gets your process number through GetTrapReason. It is stored in the upper half of the error code. The lower half contains the reason code you specify as a parameter.
 *
 * Parameters:
 *   [I] ProcessNumber (INTEGER): input
 *   [I] ReasonCode (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_435B_ForceTrap(MonContext* ctx) {
    /* TODO: Implement ForceTrap (PRT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "ProcessNumber");
    MON_LOG_IN_WORD(ctx, 1, "ReasonCode");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
