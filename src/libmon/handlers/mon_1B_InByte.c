/*
 * MON 1B [INBT/InByte]
 *
 * Reads one byte from a character device, e.g. a terminal or an opened file.
 * If the device is a word-oriented device, one word is read.
 *
 * - Bit 7 is a parity bit if terminal or file input.
 * - The program waits if there is no bytes in the input buffer of the device.
 * - The pointer to the next byte is incremented when reading from mass-storage.
 * - Background programs may read from logical device number 0 (command buffer).
 *
 * Parameters:
 *   [I] DeviceNumber (WORD): Logical device number
 *   [O] ReturnValue (WORD): Byte read (returned in I1/W1)
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_file_table.h"
#include <stdio.h>

MonResult mon_1B_InByte(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read device number */
    uint32_t device_no = mon_read_param_word(ctx, 0);

    mon_log(MON_LOG_DEBUG, MON_ID_1B ": IN: DeviceNumber=%o",
            device_no);

    int byte_read = -1;

    /* Route by device class */
    if (is_character_device(device_no) || is_terminal(device_no)) {
        /* Character device or terminal: use console I/O */
        ConsoleIO* console = mon_file_table_get_console();
        if (!console) {
            /* No console handler configured - return EOF immediately, never block */
            mon_log(MON_LOG_DEBUG, MON_ID_1B ": No console handler, returning EOF");
            mon_set_error(ctx, 57);  /* EOF error */
            return MON_ERROR;
        }

        /* Check if input is available (non-blocking) */
        if (console->char_available && !console->char_available(console->context)) {
            /* No input available - return EOF, don't block */
            mon_log(MON_LOG_DEBUG, MON_ID_1B ": No input available, EOF");
            mon_set_error(ctx, 57);  /* EOF error */
            return MON_ERROR;
        }

        if (console->read_char) {
            byte_read = console->read_char(console->context);
        } else {
            /* Fallback to stdin - note: this may still block if char_available is NULL */
            byte_read = getchar();
        }

        if (byte_read == EOF || byte_read < 0) {
            byte_read = 0;  /* Return 0 on EOF */
        }

        mon_log(MON_LOG_DEBUG, MON_ID_1B ": Read byte 0x%02X ('%c') from console",
                byte_read & 0xFF, (byte_read >= 32 && byte_read < 127) ? byte_read : '.');
    }
    else if (is_mass_storage_file(device_no)) {
        /* Mass storage file: read from open file table */
        OpenFileEntry* entry = mon_file_table_get((int)device_no);
        if (!entry || !entry->in_use) {
            mon_log(MON_LOG_WARN, MON_ID_1B ": File %o not open", device_no);
            mon_set_error(ctx, 53);  /* File not open */
            return MON_ERROR;
        }

        if (entry->host_file) {
            byte_read = fgetc(entry->host_file);
            if (byte_read == EOF) {
                byte_read = 0;  /* Return 0 on EOF */
                mon_log(MON_LOG_DEBUG, MON_ID_1B ": EOF on file %o", device_no);
            } else {
                entry->current_position++;
                mon_log(MON_LOG_DEBUG, MON_ID_1B ": Read byte 0x%02X from file %o, pos=%o",
                        byte_read, device_no, entry->current_position);
            }
        } else {
            mon_set_error(ctx, 53);
            return MON_ERROR;
        }
    }
    else {
        /* Unsupported device type */
        mon_log(MON_LOG_WARN, MON_ID_1B ": Unsupported device %o",
                device_no);
        mon_set_error(ctx, 46);  /* No such filename */
        return MON_ERROR;
    }

    /* Return byte in I1/W1 register */
    ctx->set_error_code(ctx->cpu, (uint32_t)(byte_read & 0xFF));

    /* Also write to output parameter if provided */
    if (ctx->arg_count >= 2) {
        mon_write_param_word(ctx, 1, (uint32_t)(byte_read & 0xFF));
    }

    mon_set_success(ctx);
    return MON_SUCCESS;
}
