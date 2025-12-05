/*
 * MON 502B (322 decimal): SwitchProcess (SWITCHP)
 *
 * Sets the current process in a wait state. Restarts another process. This is similar to executing a StartProcess followed by a StopProcess.
 *
 * Parameters:
 *   [I] ProcessNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_502B_SwitchProcess(MonContext* ctx) {
    /* TODO: Implement SwitchProcess (SWITCHP) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "ProcessNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
