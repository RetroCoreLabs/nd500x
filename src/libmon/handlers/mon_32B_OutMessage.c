/*
 * MON 32B [MSG/OutMessage]
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
#include "../mon_log.h"
#include "../mon_file_table.h"
#include <stdio.h>

#define MAX_MESSAGE_LEN 512

MonResult mon_32B_OutMessage(MonContext* ctx) {
    char message[MAX_MESSAGE_LEN + 1];
    int len;

    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, MON_ID_32B ": Missing parameters (need 1, got %u)", ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read the message string from parameter */
    len = mon_read_string(ctx, 0, message, MAX_MESSAGE_LEN);

    /* Output the message to user's terminal via console interface */
    /* Note: SINTRAN messages already contain \r\n terminators, don't add extra */
    if (len > 0) {
        ConsoleIO* console = mon_file_table_get_console();
        for (int i = 0; i < len; i++) {
            if (console && console->write_char) {
                console->write_char(console->context, (unsigned char)message[i]);
            } else {
                putchar(message[i]);
            }
        }
        if (!console || !console->write_char) {
            fflush(stdout);
        }
    }

    /* Log the call */
    mon_log(MON_LOG_INFO, MON_ID_32B ": IN: Message=\"%s\" (%d chars)", message, len);

    /* Set success */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
