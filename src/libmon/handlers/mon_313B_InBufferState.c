/*
 * MON 313B (203 decimal): InBufferState (IBRISZ)
 *
 * Gets information about an input buffer. The current number of bytes in it, and the number of bytes until a break character, are returned.
 * 
 * - Use ExecutionInfo to get the logical device number for terminals. You can specify 1 for your own terminal.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [O] NoInBuffer (INTEGER): output
 *   [O] NoUntilBreak (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_313B_InBufferState(MonContext* ctx) {
    /* TODO: Implement InBufferState (IBRISZ) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
