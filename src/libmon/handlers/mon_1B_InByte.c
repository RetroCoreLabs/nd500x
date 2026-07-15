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
 *   [I] DeviceNumber (INTEGER): Logical device number (0=command buffer, 1-63=char dev, 64-127=file)
 *   [O] ReturnValue (INTEGER): Byte read (written to OUTPUT param; also in I1 for ND-100 compat)
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_terminal_state.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"
#include <stdio.h>

MonResult mon_1B_InByte(MonContext* ctx) {
    /* Defensive check for argument count - need INPUT and OUTPUT params */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_1B ": Missing parameters (need 2, got %u)", ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read device number */
    uint32_t device_no = mon_read_param_word(ctx, 0);

    mon_log(MON_LOG_DEBUG, MON_ID_1B ": IN: DeviceNumber=%o",
            device_no);

    int byte_read = -1;

    /* Special case: Device 0 = Command buffer (program arguments) */
    if (device_no == 0) {
        byte_read = mon_read_command_buffer_char();
        if (byte_read == -1) {
            /* Device 0 is the SINTRAN command buffer. The ND LINKER's resident
             * reader polls it and busy-spins on EOF (it does NOT fall through to
             * the terminal on EOF), so an empty command buffer must SUSPEND like
             * a terminal read: the MON call is not committed, the CPU rewinds to
             * the CALLG, and the run loop stops with STOP_WAIT_INPUT so the host
             * can feed the next line and resume. (The command line itself is also
             * read from the terminal via 503B DVINST; both channels are fed.) */
            mon_log(MON_LOG_DEBUG, MON_ID_1B ": Command buffer empty - suspend (wait for input)");
            ctx->wait_requested = 1;
            ctx->wait_device = 0;
            return MON_SUCCESS;  /* not committed; CPU rewinds and retries */
        } else {
            mon_log(MON_LOG_DEBUG, MON_ID_1B ": Read byte 0x%02X ('%c') from command buffer",
                    byte_read & 0xFF, (byte_read >= 32 && byte_read < 127) ? byte_read : '.');
        }
    }
    /* Route by device class */
    else if (is_character_device(device_no) || is_terminal(device_no)) {
        /* Character device or terminal: use console I/O */
        ConsoleIO* console = mon_file_table_get_console();
        if (!console) {
            /* No console handler configured - return EOF immediately, never block */
            mon_log(MON_LOG_DEBUG, MON_ID_1B ": No console handler, returning EOF");
            mon_set_error(ctx, MON_ERR_END_OF_FILE);  /* 003B End of file */
            return MON_ERROR;
        }

        /* Check if input is available. On a real terminal SINTRAN SUSPENDS the
         * process when the input buffer is empty ("the program waits if there is
         * no bytes in the input buffer of the device"). We model that suspend by
         * requesting a blocking-read wait: the MON call is not committed, the CPU
         * rewinds to the CALLG, and the run loop stops with STOP_WAIT_INPUT. When
         * the host feeds input and resumes, this MON call re-executes and reads
         * the byte. This replaces the old busy-spin-on-EOF behaviour that hung
         * the linker's resident char reader when interactive input ran out. */
        if (console->char_available && !console->char_available(console->context)) {
            mon_log(MON_LOG_DEBUG, MON_ID_1B ": No input on device %o - suspend (wait for input)",
                    device_no);
            ctx->wait_requested = 1;
            ctx->wait_device = device_no;
            return MON_SUCCESS;  /* not committed; CPU rewinds and retries */
        }

        if (console->read_char) {
            byte_read = console->read_char(console->context);
        } else {
            /* Fallback to stdin - note: this may still block if char_available is NULL */
            byte_read = getchar();
        }

        /* ESCAPE (user-break) handling: if the char is this terminal's escape
         * character AND escape is ENABLED (DFLAG.5IESC clear), it is a user
         * break, not input data. If escape is DISABLED (e.g. the linker called
         * 71B DESCF), the character passes through as ordinary data. */
        if (byte_read >= 0 && mon_is_escape_break(device_no, (uint8_t)byte_read)) {
            mon_log(MON_LOG_DEBUG, MON_ID_1B ": ESCAPE (user break) on device %o", device_no);
            if (console->user_break) {
                console->user_break(console->context, device_no);
            }
            mon_set_error(ctx, MON_ERR_END_OF_FILE);  /* signal break/EOF to caller */
            return MON_ERROR;
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
            mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
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
            mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
            return MON_ERROR;
        }
    }
    else {
        /* Unsupported device type */
        mon_log(MON_LOG_WARN, MON_ID_1B ": Unsupported device %o",
                device_no);
        mon_set_error(ctx, MON_ERR_NO_SUCH_DEVICE_NAME);  /* 030B No such device name */
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
