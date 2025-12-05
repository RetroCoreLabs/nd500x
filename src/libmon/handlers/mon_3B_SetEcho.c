/*
 * MON 3B (3 decimal): SetEcho (ECHOM)
 *
 * When you press a key on the terminal, a character is normally displayed. This is called echo. You modify a terminal's echo with this monitor call.
 * 
 * - If 8-bit I/O is set (TerminalFunction, function number 112), you will get echo always if bit 7 in the echo table is set.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [I] EchoStrategy (INTEGER2): input
 *   [I] Table (INTEGER2[8]): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_3B_SetEcho(MonContext* ctx) {
    /* TODO: Implement SetEcho (ECHOM) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "EchoStrategy");
    MON_LOG_IN_WORD(ctx, 2, "Table");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
