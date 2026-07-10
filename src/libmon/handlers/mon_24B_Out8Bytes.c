/*
 * MON 24B [B8OUT/Out8Bytes]
 *
 * Writes 8 bytes to a character device, e.g. a terminal. All 8 bytes are output.
 * Unlike OutUpTo8Bytes which stops at 0 byte.
 *
 * On the ND-500, you are advised to use the faster OutputString.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): Logical device number. See appendix B.
 *   [I] OutData (STRING): Address of buffer containing 8 bytes to write.
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

MonResult mon_24B_Out8Bytes(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_24B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t data_addr = ctx->arg_addresses[1];

    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    mon_log(MON_LOG_DEBUG, MON_ID_24B ": IN: DeviceNo=%o, DataAddr=0x%08X",
            device_no, data_addr);

    /* Route by device class */
    if (is_character_device(device_no) || is_terminal(device_no)) {
        /* Character device or terminal: use console I/O */
        ConsoleIO* console = mon_file_table_get_console();

        /* Output all 8 bytes */
        for (int i = 0; i < 8; i++) {
            uint8_t b = ctx->read_byte(ctx->cpu, data_addr + i);
            if (console && console->write_char) {
                console->write_char(console->context, b);
            } else {
                putchar(b);
            }
        }

        if (!console || !console->write_char) {
            fflush(stdout);
        }

        mon_log(MON_LOG_DEBUG, MON_ID_24B ": OUT: Wrote 8 bytes to device %o", device_no);
    }
    else if (is_mass_storage_file(device_no)) {
        /* Mass storage file: write to open file table */
        OpenFileEntry* entry = mon_file_table_get((int)device_no);
        if (!entry || !entry->in_use) {
            mon_log(MON_LOG_WARN, MON_ID_24B ": File %o not open", device_no);
            mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
            return MON_ERROR;
        }

        /* Check access mode allows writing */
        if (entry->access_mode == ACCESS_SEQ_READ || entry->access_mode == ACCESS_RAND_READ) {
            mon_log(MON_LOG_WARN, MON_ID_24B ": File %o not open for writing", device_no);
            mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
            return MON_ERROR;
        }

        if (entry->host_file) {
            uint8_t buffer[8];
            for (int i = 0; i < 8; i++) {
                buffer[i] = ctx->read_byte(ctx->cpu, data_addr + i);
            }

            size_t written = fwrite(buffer, 1, 8, entry->host_file);
            if (written != 8) {
                mon_log(MON_LOG_WARN, MON_ID_24B ": Write error on file %o", device_no);
                mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
                return MON_ERROR;
            }
            fflush(entry->host_file);
            entry->current_position += 8;
            mon_log(MON_LOG_DEBUG, MON_ID_24B ": OUT: Wrote 8 bytes to file %o, pos=%o",
                    device_no, entry->current_position);
        } else {
            mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
            return MON_ERROR;
        }
    }
    else {
        /* Unsupported device type */
        mon_log(MON_LOG_WARN, MON_ID_24B ": Unsupported device %o", device_no);
        mon_set_error(ctx, MON_ERR_NO_SUCH_DEVICE_NAME);  /* 030B No such device name */
        return MON_ERROR;
    }

    mon_set_success(ctx);
    return MON_SUCCESS;
}
