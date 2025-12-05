/*
 * MON 32B (26 decimal): OutMessage (MSG)
 *
 * Writes a message to the user's terminal. This is convenient for error
 * messages in background programs.
 *
 * Parameters:
 *   [I] Message (STRING): String message to write to user's terminal
 *       (max 512 characters).
 *
 * Reference: ND-860228.2 EN (SINTRAN III Monitor Calls)
 */

#include "../mon.h"
#include <stdio.h>

#define MAX_MESSAGE_LEN 512

MonResult mon_32B_OutMessage(MonContext* ctx) {
    char message[MAX_MESSAGE_LEN + 1];
    int len;

    /* Read the message string from parameter */
    len = mon_read_string(ctx, 0, message, MAX_MESSAGE_LEN);

    /* Output the message to stdout (user's terminal) */
    if (len > 0) {
        fprintf(stdout, "%s\n", message);
        fflush(stdout);
    }

    /* Log the call */
    mon_log(MON_LOG_INFO, "MON 32B MSG: \"%s\" (%d chars)", message, len);

    /* Set success */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
