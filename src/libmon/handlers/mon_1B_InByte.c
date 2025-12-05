/*
 * MON 1B (1 decimal): InByte (INBT)
 *
 * Reads one byte from a character device, e.g. a terminal or an opened file.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): Logical device number (1=stdin)
 *   [O] ReturnValue (INTEGER): The byte read (low 8 bits)
 *
 * Device numbers:
 *   0 = SINTRAN command buffer
 *   1 = Terminal input (stdin)
 *   Others = File handles
 *
 * K flag: Set on error (EOF, device not ready)
 *
 * IMPLEMENTATION STATUS: VALIDATED (terminal input only)
 */

#include "../mon.h"
#include <stdio.h>

MonResult mon_1B_InByte(MonContext* ctx) {
    /* Validate parameters */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_ERROR, "INBT: Expected 2 parameters, got %u", ctx->arg_count);
        mon_set_error(ctx, -1);
        return MON_ERROR;
    }

    /* Read device number */
    uint32_t device = mon_read_param_word(ctx, 0);

    /* Log input parameter */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Handle device 1 = terminal (stdin) */
    if (device == 1) {
        int ch = getchar();

        if (ch == EOF) {
            mon_log(MON_LOG_WARN, "INBT: EOF on stdin");
            mon_set_error(ctx, -2);  /* EOF error */
            return MON_ERROR;
        }

        /* Write byte to output parameter (as word with byte in low 8 bits) */
        mon_write_param_word(ctx, 1, (uint32_t)(ch & 0xFF));

        /* Log output */
        MON_LOG_OUT_WORD(ctx, 1, "ReturnValue");

        mon_set_success(ctx);
        return MON_SUCCESS;
    }

    /* Device 0 = command buffer (not yet implemented) */
    if (device == 0) {
        mon_log(MON_LOG_WARN, "INBT: Command buffer (device 0) not implemented");
        mon_set_error(ctx, -3);
        return MON_ERROR;
    }

    /* TODO: Handle file handles */
    mon_log(MON_LOG_ERROR, "INBT: Unsupported device %u", device);
    mon_set_error(ctx, -4);  /* Invalid device */
    return MON_ERROR;
}
