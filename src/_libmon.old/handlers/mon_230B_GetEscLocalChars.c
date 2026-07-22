/*
 * MON 230B (152 decimal): GetEscLocalChars (MGDAE)
 *
 * Gets ESCAPE and LOCAL characters. You can terminate most programs with the ESCAPE key. A LOCAL key has a similar function. It terminates a connection to a remote computer in a COSMOS network. The system supervisor may select other keys for these functions. This monitor call tells you which keys to use.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): input
 *   [O] DisconnectChar (INTEGER): output
 *   [O] EscapeChar (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_230B_GetEscLocalChars(MonContext* ctx) {
    /* TODO: Implement GetEscLocalChars (MGDAE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
