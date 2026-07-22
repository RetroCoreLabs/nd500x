/*
 * MON 332B (218 decimal): TerminalLineInfo (TREPP)
 *
 * Gets information about a terminal line. You may also enable programs to continue in spite of errors on the terminal line.
 *
 * Parameters:
 *   [I] FunctionCode (INTEGER): input
 *   [I] DeviceNo (INTEGER): input
 *   [O] ReturnInfo (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_332B_TerminalLineInfo(MonContext* ctx) {
    /* TODO: Implement TerminalLineInfo (TREPP) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FunctionCode");
    MON_LOG_IN_WORD(ctx, 1, "DeviceNo");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
