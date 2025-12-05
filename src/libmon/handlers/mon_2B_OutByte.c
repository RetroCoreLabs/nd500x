/*
 * MON 2B (2 decimal): OutByte (OUTBT)
 *
 * Writes one byte to a character device, e.g. a terminal or an opened file.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): Logical device number (1=stdout, 2=stderr)
 *   [I] OutputValue (INTEGER): The byte to write (low 8 bits)
 *
 * Device numbers:
 *   1 = Terminal output (stdout)
 *   2 = Error output (stderr)
 *   Others = File handles
 *
 * K flag: Set on error (device not ready)
 *
 * IMPLEMENTATION STATUS: VALIDATED (terminal output only)
 */

#include "../mon.h"
#include <stdio.h>

MonResult mon_2B_OutByte(MonContext* ctx) {
    /* Validate parameters */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_ERROR, "OUTBT: Expected 2 parameters, got %u", ctx->arg_count);
        mon_set_error(ctx, -1);
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t device = mon_read_param_word(ctx, 0);
    uint32_t value = mon_read_param_word(ctx, 1);
    uint8_t byte = (uint8_t)(value & 0xFF);

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "OutputValue");

    /* Handle device 1 = terminal stdout */
    if (device == 1) {
        int result = putchar((int)byte);
        fflush(stdout);

        if (result == EOF) {
            mon_log(MON_LOG_ERROR, "OUTBT: Failed to write to stdout");
            mon_set_error(ctx, -2);
            return MON_ERROR;
        }

        mon_set_success(ctx);
        return MON_SUCCESS;
    }

    /* Handle device 2 = stderr */
    if (device == 2) {
        int result = fputc((int)byte, stderr);
        fflush(stderr);

        if (result == EOF) {
            mon_log(MON_LOG_ERROR, "OUTBT: Failed to write to stderr");
            mon_set_error(ctx, -2);
            return MON_ERROR;
        }

        mon_set_success(ctx);
        return MON_SUCCESS;
    }

    /* TODO: Handle file handles */
    mon_log(MON_LOG_ERROR, "OUTBT: Unsupported device %u", device);
    mon_set_error(ctx, -3);  /* Invalid device */
    return MON_ERROR;
}
