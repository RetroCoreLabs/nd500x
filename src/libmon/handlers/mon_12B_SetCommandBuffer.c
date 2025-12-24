/*
 * MON 12B [SETCM/SetCommandBuffer]
 *
 * Transfers a string to the command buffer. The command buffer contains the
 * last command input from the terminal. You may read the command buffer by
 * reading from logical device number 0.
 *
 * - The command @TERMINAL-STATISTICS lists the command buffer.
 * - You may use this to erase sensitive information like passwords.
 *
 * Parameters:
 *   [I] Command (STRING): Command string to set
 *
 * THREAD SAFETY: Uses shared command buffer functions from mon_file_table.c
 * which have static global state without mutex protection. External
 * synchronization required if accessed from multiple threads.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_file_table.h"

MonResult mon_12B_SetCommandBuffer(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, MON_ID_12B ": Missing parameters (need 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read command string */
    char command[256];
    mon_read_string(ctx, 0, command, sizeof(command));

    mon_log(MON_LOG_DEBUG, MON_ID_12B ": IN: Command='%s'", command);

    /* Store in shared command buffer */
    mon_set_command_buffer(command);

    mon_log(MON_LOG_DEBUG, MON_ID_12B ": OUT: CommandBuffer='%s'", mon_get_command_buffer());

    mon_set_success(ctx);
    return MON_SUCCESS;
}
