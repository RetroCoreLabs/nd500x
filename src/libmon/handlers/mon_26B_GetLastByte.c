/*
 * MON 26B (22 decimal): GetLastByte (LASTC)
 *
 * Gets the last character typed on a terminal. The monitor call can be used to terminate long output sequences, reading from files, etc. The program never enters the I/O wait state because of this monitor call.
 * 
 * - Only user SYSTEM can execute this monitor call on a non-reserved terminal.
 * - This monitor call always returns -1 if an error is encountered, not the standard SINTRAN error codes.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [O] LastCharTyped (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_26B_GetLastByte(MonContext* ctx) {
    /* TODO: Implement GetLastByte (LASTC) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
