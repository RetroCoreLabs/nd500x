/*
 * MON 306B (198 decimal): GetTerminalMode (GTMOD)
 *
 * Gets the terminal mode. The terminal mode tells how the terminal function, i.e. if all letters are converted to uppercase, if output stops when a full page is displayed, etc.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [O] TerminalMode (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_306B_GetTerminalMode(MonContext* ctx) {
    /* TODO: Implement GetTerminalMode (GTMOD) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
