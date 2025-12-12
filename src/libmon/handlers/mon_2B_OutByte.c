/*
 * MON 2B (2 decimal): OutByte (OUTBT)
 *
 * Writes one byte to a character device, e.g. a terminal or an opened file.
 * If the device is a word-oriented device, one word is written.
 *
 * - The program waits if the output buffer of the device is full.
 * - The pointer to the next byte is incremented when writing to mass-storage.
 * - You are advised to use the faster OutputString on the ND-500.
 *
 * Parameters:
 *   [I] DeviceNumber (WORD): Logical device number
 *   [I] OutputValue (WORD): Byte to write (low 8 bits used)
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_file_table.h"
#include <stdio.h>

MonResult mon_2B_OutByte(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 2) {
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t output_value = mon_read_param_word(ctx, 1);
    uint8_t byte_out = (uint8_t)(output_value & 0xFF);

    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "OutputValue");

    mon_log(MON_LOG_DEBUG, "MON 2B OUTBT: DeviceNumber=%u (octal %o), OutputValue=0x%02X ('%c')",
            device_no, device_no, byte_out,
            (byte_out >= 32 && byte_out < 127) ? byte_out : '.');

    /* Route by device class */
    if (is_character_device(device_no) || is_terminal(device_no)) {
        /* Character device or terminal: use console I/O */
        ConsoleIO* console = mon_file_table_get_console();
        if (console && console->write_char) {
            console->write_char(console->context, byte_out);
        } else {
            /* Fallback to stdout */
            putchar(byte_out);
            fflush(stdout);
        }

        mon_log(MON_LOG_DEBUG, "MON 2B OUTBT: Wrote byte 0x%02X to console", byte_out);
    }
    else if (is_mass_storage_file(device_no)) {
        /* Mass storage file: write to open file table */
        OpenFileEntry* entry = mon_file_table_get((int)device_no);
        if (!entry || !entry->in_use) {
            mon_log(MON_LOG_WARN, "MON 2B OUTBT: File %u not open", device_no);
            mon_set_error(ctx, 53);  /* File not open */
            return MON_ERROR;
        }

        /* Check access mode allows writing */
        if (entry->access_mode == ACCESS_SEQ_READ || entry->access_mode == ACCESS_RAND_READ) {
            mon_log(MON_LOG_WARN, "MON 2B OUTBT: File %u not open for writing", device_no);
            mon_set_error(ctx, 52);  /* Invalid parameter (wrong access mode) */
            return MON_ERROR;
        }

        if (entry->host_file) {
            if (fputc(byte_out, entry->host_file) == EOF) {
                mon_log(MON_LOG_WARN, "MON 2B OUTBT: Write error on file %u", device_no);
                mon_set_error(ctx, 52);
                return MON_ERROR;
            }
            entry->current_position++;
            mon_log(MON_LOG_DEBUG, "MON 2B OUTBT: Wrote byte 0x%02X to file %u, pos=%u",
                    byte_out, device_no, entry->current_position);
        } else {
            mon_set_error(ctx, 53);
            return MON_ERROR;
        }
    }
    else {
        /* Unsupported device type */
        mon_log(MON_LOG_WARN, "MON 2B OUTBT: Unsupported device %u (octal %o)",
                device_no, device_no);
        mon_set_error(ctx, 46);  /* No such filename */
        return MON_ERROR;
    }

    mon_set_success(ctx);
    return MON_SUCCESS;
}
