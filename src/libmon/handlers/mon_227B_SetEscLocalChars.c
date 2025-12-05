/*
 * MON 227B (151 decimal): SetEscLocalChars (MSDAE)
 *
 * You can terminate most programs with the ESCAPE key. A LOCAL key has a similar function. It terminates a connection to a remote computer in a network. This monitor call allows you to select other keys for these functions.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): input
 *   [I] DisconnectChar (INTEGER): input
 *   [I] EscapeChar (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_227B_SetEscLocalChars(MonContext* ctx) {
    /* TODO: Implement SetEscLocalChars (MSDAE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "DisconnectChar");
    MON_LOG_IN_WORD(ctx, 2, "EscapeChar");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
