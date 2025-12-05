/*
 * MON 32B (26 decimal): OutMessage (MSG)
 *
 * Writes a message to the user's terminal. This is convenient for error messages in background programs.
 *
 * Parameters:
 *   [I] Message (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_32B_OutMessage(MonContext* ctx) {
    /* TODO: Implement OutMessage (MSG) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Message");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
