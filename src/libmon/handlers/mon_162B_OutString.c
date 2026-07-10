/*
 * MON 162B [OUTST/OutString]
 *
 * Writes a string of characters to a peripheral file, e.g., a terminal or a printer.
 *
 * - You cannot use this monitor call for mass-storage files.
 * - The output buffer of the device may be too small. Then the program waits
 *   until the required buffer space becomes available.
 * - Parameters are fetched and returned through the alternative page table.
 * - The maximum string length is 2048 bytes (as in OutputString).
 * - For performance reasons, it is inadvisable to use this call from the ND-500.
 *   Use OutputString (504B) instead.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): Logical device number. Cannot use 1 for own terminal.
 *       Use ExecutionInfo to get its logical device number. File numbers are illegal.
 *   [I] TextWrite (STRING): Address of character string to be output (max 2048 bytes).
 *   [I] NoOfBytes (INTEGER): Number of characters to write.
 *   [O] ReturnStatus (INTEGER): Return status (output).
 *
 * Returns:
 *   K flag set on error, error code in I1
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"
#include <stdio.h>

#define MAX_OUTSTRING_LENGTH 2048

MonResult mon_162B_OutString(MonContext* ctx) {
    /* Defensive check for argument count (need at least 3 parameters) */
    if (ctx->arg_count < 3) {
        mon_log(MON_LOG_WARN, MON_ID_162B ": Missing parameters (need 3, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t text_addr = ctx->arg_addresses[1];
    uint32_t no_of_bytes = mon_read_param_word(ctx, 2);

    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 2, "NoOfBytes");

    mon_log(MON_LOG_DEBUG, MON_ID_162B ": IN: DeviceNo=%o, TextAddr=0x%08X, NoOfBytes=%o",
            device_no, text_addr, no_of_bytes);

    /* Clamp to max length */
    if (no_of_bytes > MAX_OUTSTRING_LENGTH) {
        mon_log(MON_LOG_WARN, MON_ID_162B ": NoOfBytes %o exceeds max %o, clamping",
                no_of_bytes, MAX_OUTSTRING_LENGTH);
        no_of_bytes = MAX_OUTSTRING_LENGTH;
    }

    /* File numbers are illegal for this call */
    if (is_mass_storage_file(device_no)) {
        mon_log(MON_LOG_WARN, MON_ID_162B ": File numbers not allowed (device %o)", device_no);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */

        /* Write error status to output parameter if provided */
        if (ctx->arg_count >= 4) {
            mon_write_param_word(ctx, 3, 52);
        }

        return MON_ERROR;
    }

    /* Validate device type */
    if (!is_character_device(device_no) && !is_terminal(device_no)) {
        mon_log(MON_LOG_WARN, MON_ID_162B ": Unsupported device %o", device_no);
        mon_set_error(ctx, MON_ERR_NO_SUCH_DEVICE_NAME);  /* 030B No such device name */

        /* Write error status to output parameter if provided */
        if (ctx->arg_count >= 4) {
            mon_write_param_word(ctx, 3, 46);
        }

        return MON_ERROR;
    }

    if (no_of_bytes == 0) {
        /* Nothing to write */
        if (ctx->arg_count >= 4) {
            mon_write_param_word(ctx, 3, 0);  /* Success */
        }
        mon_set_success(ctx);
        return MON_SUCCESS;
    }

    /* Get console I/O handler */
    ConsoleIO* console = mon_file_table_get_console();

    /* Output the string */
    for (uint32_t i = 0; i < no_of_bytes; i++) {
        uint8_t b = ctx->read_byte(ctx->cpu, text_addr + i);
        if (console && console->write_char) {
            console->write_char(console->context, b);
        } else {
            putchar(b);
        }
    }

    if (!console || !console->write_char) {
        fflush(stdout);
    }

    /* Write success status to output parameter if provided */
    if (ctx->arg_count >= 4) {
        mon_write_param_word(ctx, 3, 0);  /* 0 = success */
    }

    mon_log(MON_LOG_DEBUG, MON_ID_162B ": OUT: Wrote %o bytes to device %o", no_of_bytes, device_no);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
