/*
 * MON 503B [DVINST/InputString]
 *
 * Reads a string from a device, e.g. a terminal or an opened file.
 * This monitor call provides fast input to ND-500 programs.
 *
 * - Maximum string length is 2048 bytes.
 * - Break strategies control when input terminates (e.g., on newline).
 * - Echo strategies control how input is echoed back.
 *
 * Parameters:
 *   [I] DevNo (INTEGER): Logical device number
 *   [I] MaxNo (INTEGER): Maximum bytes to read (max 2048)
 *   [O] NoOfBytesRet (INTEGER): Number of bytes actually read
 *   [O] Buff (STRING): Buffer to store input string
 *   [I] BreakStrat (INTEGER): Break strategy (0=standard, uses BreakT1-T4)
 *   [I] EchoStrat (INTEGER): Echo strategy (0=standard, uses EchoT1-T4)
 *   [I] BreakT1-T4 (INTEGER): Break characters (up to 4 terminators)
 *   [I] EchoT1-T4 (INTEGER): Echo table entries (not implemented)
 *
 * Break Strategy:
 *   - 0: Use break table (BreakT1-T4 define terminating characters)
 *   - Default break characters: CR (0x0D), LF (0x0A), 0x27 (SINTRAN string end)
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_file_table.h"
#include <stdio.h>
#include <string.h>

#define DVINST_MAX_BYTES 2048

/* Default break characters if none specified */
#define BREAK_CR    0x0D  /* Carriage return */
#define BREAK_LF    0x0A  /* Line feed */
#define BREAK_END   0x27  /* SINTRAN string terminator */

/* Check if character is a break character */
static inline bool is_break_char(uint8_t c, const uint8_t break_chars[4]) {
    return (c == break_chars[0] || c == break_chars[1] ||
            c == break_chars[2] || c == break_chars[3]);
}

MonResult mon_503B_InputString(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 4) {
        mon_log(MON_LOG_WARN, MON_ID_503B ": Missing parameters (need at least 4, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t max_bytes = mon_read_param_word(ctx, 1);
    uint32_t ret_count_addr = ctx->arg_addresses[2];  /* Output: bytes read */
    uint32_t buffer_addr = ctx->arg_addresses[3];     /* Output: string buffer */

    /* Optional break/echo strategy parameters */
    uint32_t break_strat = (ctx->arg_count > 4) ? mon_read_param_word(ctx, 4) : 0;
    uint32_t echo_strat = (ctx->arg_count > 5) ? mon_read_param_word(ctx, 5) : 0;

    /* Break characters (default to standard terminators) */
    uint8_t break_chars[4] = { BREAK_CR, BREAK_LF, BREAK_END, 0 };
    if (ctx->arg_count > 6) {
        break_chars[0] = (uint8_t)(mon_read_param_word(ctx, 6) & 0xFF);
    }
    if (ctx->arg_count > 7) {
        break_chars[1] = (uint8_t)(mon_read_param_word(ctx, 7) & 0xFF);
    }
    if (ctx->arg_count > 8) {
        break_chars[2] = (uint8_t)(mon_read_param_word(ctx, 8) & 0xFF);
    }
    if (ctx->arg_count > 9) {
        break_chars[3] = (uint8_t)(mon_read_param_word(ctx, 9) & 0xFF);
    }

    MON_LOG_IN_WORD(ctx, 0, "DevNo");
    MON_LOG_IN_WORD(ctx, 1, "MaxNo");

    mon_log(MON_LOG_DEBUG, MON_ID_503B ": IN: DevNo=%o, MaxNo=%o, BuffAddr=0x%08X",
            device_no, max_bytes, buffer_addr);

    /* Validate byte count */
    if (max_bytes > DVINST_MAX_BYTES) {
        mon_log(MON_LOG_WARN, MON_ID_503B ": MaxNo %o exceeds max %o", max_bytes, DVINST_MAX_BYTES);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    if (max_bytes == 0) {
        /* Nothing to read */
        mon_write_param_word(ctx, 2, 0);  /* NoOfBytesRet = 0 */
        mon_set_success(ctx);
        return MON_SUCCESS;
    }

    uint8_t buffer[DVINST_MAX_BYTES];
    uint32_t bytes_read = 0;
    int ch;

    /* Route by device class */
    if (is_character_device(device_no) || is_terminal(device_no)) {
        /* Character device or terminal: use console I/O */
        ConsoleIO* console = mon_file_table_get_console();

        while (bytes_read < max_bytes) {
            if (console && console->read_char) {
                ch = console->read_char(console->context);
            } else {
                /* Fallback to stdin */
                ch = getchar();
            }

            if (ch == EOF) {
                break;
            }

            buffer[bytes_read++] = (uint8_t)ch;

            /* Echo if strategy allows (simplified: always echo for strategy 0) */
            if (echo_strat == 0) {
                if (console && console->write_char) {
                    console->write_char(console->context, ch);
                } else {
                    putchar(ch);
                }
            }

            /* Check for break character */
            if (is_break_char((uint8_t)ch, break_chars)) {
                break;
            }
        }

        if (!console || !console->write_char) {
            fflush(stdout);
        }

        mon_log(MON_LOG_DEBUG, MON_ID_503B ": Read %o bytes from console (device %o)",
                bytes_read, device_no);
    }
    else if (is_mass_storage_file(device_no)) {
        /* Mass storage file: read from open file table */
        OpenFileEntry* entry = mon_file_table_get((int)device_no);
        if (!entry || !entry->in_use) {
            mon_log(MON_LOG_WARN, MON_ID_503B ": File %o not open", device_no);
            mon_set_error(ctx, 53);  /* File not open */
            return MON_ERROR;
        }

        /* Check access mode allows reading */
        if (entry->access_mode == ACCESS_SEQ_WRITE || entry->access_mode == ACCESS_SEQ_APPEND) {
            mon_log(MON_LOG_WARN, MON_ID_503B ": File %o not open for reading", device_no);
            mon_set_error(ctx, 52);  /* Invalid parameter (wrong access mode) */
            return MON_ERROR;
        }

        if (entry->host_file) {
            while (bytes_read < max_bytes) {
                ch = fgetc(entry->host_file);
                if (ch == EOF) {
                    break;
                }

                buffer[bytes_read++] = (uint8_t)ch;
                entry->current_position++;

                /* Check for break character (for file I/O too) */
                if (is_break_char((uint8_t)ch, break_chars)) {
                    break;
                }
            }
            mon_log(MON_LOG_DEBUG, MON_ID_503B ": Read %o bytes from file %o, pos=%o",
                    bytes_read, device_no, entry->current_position);
        } else {
            mon_set_error(ctx, 53);
            return MON_ERROR;
        }
    }
    else {
        /* Unsupported device type */
        mon_log(MON_LOG_WARN, MON_ID_503B ": Unsupported device %o", device_no);
        mon_set_error(ctx, 46);  /* No such filename */
        return MON_ERROR;
    }

    /* Write buffer to emulator memory */
    for (uint32_t i = 0; i < bytes_read; i++) {
        ctx->write_byte(ctx->cpu, buffer_addr + i, buffer[i]);
    }

    /* Return number of bytes read */
    mon_write_param_word(ctx, 2, bytes_read);

    mon_log(MON_LOG_DEBUG, MON_ID_503B ": OUT: Returned %o bytes", bytes_read);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
