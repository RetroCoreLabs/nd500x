/*
 * MON 22B [M8OUT/OutUpTo8Bytes]
 *
 * Writes up to 8 characters to a device, e.g. a terminal or an internal device.
 * Writing terminates when a character with value 0 is found. The 0 byte is not output.
 *
 * Note: Only for terminals, TADs, internal devices, and synchronous modems.
 * File numbers are illegal.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): Logical device number. Use 1 for your own terminal.
 *   [I] OutData (STRING): Address of 8 characters to be written (stops at first 0 byte).
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

MonResult mon_22B_OutUpTo8Bytes(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_22B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t data_addr = ctx->arg_addresses[1];

    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");

    mon_log(MON_LOG_DEBUG, MON_ID_22B ": IN: DeviceNo=%o, DataAddr=0x%08X",
            device_no, data_addr);

    /* File numbers are illegal for this call */
    if (is_mass_storage_file(device_no)) {
        mon_log(MON_LOG_WARN, MON_ID_22B ": File numbers not allowed (device %o)", device_no);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    /* Validate device type */
    if (!is_character_device(device_no) && !is_terminal(device_no)) {
        mon_log(MON_LOG_WARN, MON_ID_22B ": Unsupported device %o", device_no);
        mon_set_error(ctx, MON_ERR_NO_SUCH_DEVICE_NAME);  /* 030B No such device name */
        return MON_ERROR;
    }

    /* Get console I/O handler */
    ConsoleIO* console = mon_file_table_get_console();

    /* Read and output up to 8 bytes, stopping at 0 */
    int bytes_written = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t b = ctx->read_byte(ctx->cpu, data_addr + i);
        if (b == 0) {
            break;  /* Stop at 0 byte */
        }

        if (console && console->write_char) {
            console->write_char(console->context, b);
        } else {
            putchar(b);
        }
        bytes_written++;
    }

    if (!console || !console->write_char) {
        fflush(stdout);
    }

    mon_log(MON_LOG_DEBUG, MON_ID_22B ": OUT: Wrote %d bytes to device %o", bytes_written, device_no);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
