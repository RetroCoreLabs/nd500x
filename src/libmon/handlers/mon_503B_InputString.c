/*
 * MON 503B [DVINST/InputString]
 *
 * Reads a string from a device, e.g. a terminal or an opened file.
 * This monitor call provides fast input to ND-500 programs.
 *
 * - Maximum string length is 2048 bytes.
 * - Break strategies control when input terminates.
 * - Echo strategies control how input is echoed back.
 *
 * Parameters:
 *   [I] DevNo (INTEGER): Logical device number
 *   [I] MaxNo (INTEGER): Maximum bytes to read (max 2048)
 *   [O] NoOfBytesRet (INTEGER): Number of bytes actually read
 *   [O] Buff (STRING): Buffer to store input string
 *   [I] BreakStrat (INTEGER): Break strategy for this call
 *   [I] EchoStrat (INTEGER): Echo strategy for this call
 *   [I] BreakT1-T4 (INTEGER): 128-bit break table (strategy 7 or 8)
 *   [I] EchoT1-T4 (INTEGER): 128-bit echo table (strategy 7 or 8)
 *
 * Break Strategy Values (per-call override):
 *   <0: No break - input continues until max chars or EOF
 *    0: All characters break - every character terminates input
 *    1: Control characters (0-31) break
 *    2: MAC (machine code) - CR, LF, ESC, EOF, 0x27
 *  3-6: System-defined tables
 *    7: User-defined 128-bit table (uses BreakT1-T4)
 *    8: Last user-defined table
 *    9: Max chars only - break only when MaxNo reached
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include <stdlib.h>
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"
#include "../mon_terminal_state.h"
#include <stdio.h>
#include <string.h>

#define DVINST_MAX_BYTES 2048

/* Check if character is a break character based on strategy and tables */
static bool is_break_char_ex(uint8_t ch, int32_t strategy, const BitTable128* user_table,
                              bool eight_bit_io) {
    /* 8-bit I/O mode special handling for chars >= 128 */
    if (eight_bit_io && ch >= 128) {
        if (user_table && bit_table_test_bit(user_table, 7)) {
            return true;  /* All high-bit chars break */
        }
        return false;
    }

    switch (strategy) {
        case BREAK_STRAT_NONE:
            return false;
        case BREAK_STRAT_ALL:
            return true;
        case BREAK_STRAT_CONTROL:
            return ch < 32;
        case BREAK_STRAT_MAC:
            return (ch == BREAK_CHAR_CR || ch == BREAK_CHAR_LF ||
                    ch == BREAK_CHAR_ESC || ch == BREAK_CHAR_EOF ||
                    ch == BREAK_CHAR_END);
        case BREAK_STRAT_USER:
        case BREAK_STRAT_LAST_USER:
            /* Strategy 7 and 8 both use same user table */
            if (user_table) {
                return bit_table_test_bit(user_table, ch);
            }
            return false;
        case BREAK_STRAT_MAX_ONLY:
            /* Strategy 9: No character breaks - only count-based */
            return false;
        default:
            /* System tables (3-6) and unknown - PLACEHOLDER: use MAC */
            return (ch == BREAK_CHAR_CR || ch == BREAK_CHAR_LF ||
                    ch == BREAK_CHAR_ESC || ch == BREAK_CHAR_EOF ||
                    ch == BREAK_CHAR_END);
    }
}

/* Check if character should be echoed based on strategy and tables.
 * IMPORTANT: Echo table has INVERTED semantics from break table!
 * bit=0 means echo, bit=1 means don't echo. */
static bool should_echo_char_ex(uint8_t ch, int32_t strategy, const BitTable128* user_table,
                                 bool eight_bit_io) {
    /* 8-bit I/O mode special handling for chars >= 128 */
    if (eight_bit_io && ch >= 128) {
        if (user_table && bit_table_test_bit(user_table, 7)) {
            return true;  /* All high-bit chars echoed */
        }
        return false;
    }

    switch (strategy) {
        case ECHO_STRAT_NONE:
            return false;
        case ECHO_STRAT_ALL:
            return true;
        case ECHO_STRAT_NO_CONTROL:
            return ch >= 32 && ch < 127;
        case ECHO_STRAT_MAC:
            return (ch >= 32 && ch < 127) ||
                   ch == BREAK_CHAR_CR || ch == BREAK_CHAR_LF;
        case ECHO_STRAT_USER:
        case ECHO_STRAT_LAST_USER:
            if (user_table) {
                /* INVERTED: bit=0 means echo, bit=1 means don't echo */
                return !bit_table_test_bit(user_table, ch);
            }
            return true;
        default:
            /* System tables and unknown - echo all except control */
            return ch >= 32 && ch < 127;
    }
}

MonResult mon_503B_InputString(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 4) {
        mon_log(MON_LOG_WARN, MON_ID_503B ": Missing parameters (need at least 4, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    /* TEMP Phase-2 arg-layout probe (remove after mapping the linker's DVINST) */
    if (getenv("ND500X_DVINST_DUMP")) {
        for (uint32_t _i = 0; _i < ctx->arg_count && _i < 16; _i++) {
            mon_log(MON_LOG_WARN, "  DVINST arg[%u] @0x%08X = 0x%08X",
                    _i, ctx->arg_addresses[_i], mon_read_param_word(ctx, (int)_i));
        }
    }
    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t max_bytes = mon_read_param_word(ctx, 1);
    uint32_t ret_count_addr = ctx->arg_addresses[2];  /* Output: bytes read */
    uint32_t buffer_addr = ctx->arg_addresses[3];     /* Output: string buffer */

    /* Per-call break/echo strategy parameters */
    int32_t break_strat = (ctx->arg_count > 4)
                          ? (int32_t)mon_read_param_word(ctx, 4)
                          : mon_get_break_strategy(device_no);  /* Use device default */
    int32_t echo_strat = (ctx->arg_count > 5)
                          ? (int32_t)mon_read_param_word(ctx, 5)
                          : mon_get_echo_strategy(device_no);   /* Use device default */

    /* Get 8-bit I/O mode from device state */
    bool eight_bit_io = mon_get_eight_bit_io(device_no);

    /* Break table handling:
     * Strategy 7 or 8 with inline T1-T4: read and store to device state
     * Strategy 7 or 8 without inline: use device's current user table
     * Note: DVINST docs say "Use 8 for user-defined break table" */
    const BitTable128* break_table_ptr = NULL;
    if ((break_strat == BREAK_STRAT_USER || break_strat == BREAK_STRAT_LAST_USER)
        && ctx->arg_count > 9) {
        /* Strategy 7 or 8 with inline tables - read and store to device state */
        uint32_t words[4];
        words[0] = mon_read_param_word(ctx, 6);
        words[1] = mon_read_param_word(ctx, 7);
        words[2] = mon_read_param_word(ctx, 8);
        words[3] = mon_read_param_word(ctx, 9);
        mon_update_user_break_table(device_no, words);  /* Store to device */
        break_table_ptr = mon_get_user_break_table(device_no);
    }
    else if (break_strat == BREAK_STRAT_USER || break_strat == BREAK_STRAT_LAST_USER) {
        /* Strategy 7 or 8 without inline: use current device table */
        break_table_ptr = mon_get_user_break_table(device_no);
    }

    /* Guard against a mis-decoded user break table. The ND LINKER's 503B DVINST
     * uses a 14-arg layout that differs from NC's (arg[1] is a procedure pointer,
     * not MaxNo; args[6..9] are not an inline break table - see MON_TO_BINARY_PLAN
     * "503B DVINST arg-layout mismatch"). Read positionally, its "table" is garbage
     * that breaks on the letter 'T' but NOT on CR, so every command truncates at
     * its first 'T'. A valid terminal line-input break table ALWAYS breaks on CR
     * (0x0D). If a user-table strategy yields a table that does not break on CR,
     * treat it as invalid and fall back to MAC-style line breaks (CR/LF/ESC/EOF).
     * NC uses strategy 1 and is unaffected. (Remove once the linker's true 503B
     * parameter layout is mapped and the args are read correctly.) */
    if ((break_strat == BREAK_STRAT_USER || break_strat == BREAK_STRAT_LAST_USER) &&
        (!break_table_ptr || !bit_table_test_bit(break_table_ptr, BREAK_CHAR_CR))) {
        mon_log(MON_LOG_WARN, MON_ID_503B ": user break table does not break on CR"
                " - falling back to MAC-style line break (unresolved 503B arg layout)");
        break_strat = BREAK_STRAT_MAC;
        break_table_ptr = NULL;
    }

    /* Echo table handling:
     * Strategy 7 or 8 with inline T1-T4: read and store to device state
     * Strategy 7 or 8 without inline: use device's current user table
     * Note: DVINST docs say "Use 8 for user-defined echo table" */
    const BitTable128* echo_table_ptr = NULL;
    if ((echo_strat == ECHO_STRAT_USER || echo_strat == ECHO_STRAT_LAST_USER)
        && ctx->arg_count > 13) {
        /* Strategy 7 or 8 with inline tables - read and store to device state */
        uint32_t words[4];
        words[0] = mon_read_param_word(ctx, 10);
        words[1] = mon_read_param_word(ctx, 11);
        words[2] = mon_read_param_word(ctx, 12);
        words[3] = mon_read_param_word(ctx, 13);
        mon_update_user_echo_table(device_no, words);  /* Store to device */
        echo_table_ptr = mon_get_user_echo_table(device_no);
    }
    else if (echo_strat == ECHO_STRAT_USER || echo_strat == ECHO_STRAT_LAST_USER) {
        /* Strategy 7 or 8 without inline: use current device table */
        echo_table_ptr = mon_get_user_echo_table(device_no);
    }

    MON_LOG_IN_WORD(ctx, 0, "DevNo");
    MON_LOG_IN_WORD(ctx, 1, "MaxNo");

    /* Identify device type */
    const char* dev_type = "unknown";
    if (is_character_device(device_no)) dev_type = "character";
    else if (is_terminal(device_no)) dev_type = "terminal";
    else if (is_mass_storage_file(device_no)) dev_type = "file";

    mon_log(MON_LOG_DEBUG, MON_ID_503B ": IN: DevNo=%u (%o), MaxNo=%u, BuffAddr=0x%08X",
            device_no, device_no, max_bytes, buffer_addr);
    mon_log(MON_LOG_DEBUG, MON_ID_503B ":     DeviceType=%s, BreakStrat=%d, EchoStrat=%d",
            dev_type, break_strat, echo_strat);

    /* Validate byte count. PHASE-2 EXPERIMENT (session 557c0950): the ND LINKER
     * passes a huge MaxNo (0xF80000CB); clamp instead of erroring to test whether
     * its command read then proceeds (BuffAddr correct?) - revert if not. */
    if (max_bytes > DVINST_MAX_BYTES) {
        mon_log(MON_LOG_WARN, MON_ID_503B ": MaxNo %u exceeds max %u - clamping", max_bytes, DVINST_MAX_BYTES);
        max_bytes = DVINST_MAX_BYTES;
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

        /* Blocking line-input: a terminal DVINST with NO input available must
         * SUSPEND the process (SINTRAN "the program waits ...") rather than
         * return an empty line, which would make the caller act on a blank
         * answer and busy-loop. Mirror the MON 1B blocking-read model: since
         * nothing has been consumed yet, the MON call is not committed - the
         * CPU rewinds to the CALLG and the run loop stops with STOP_WAIT_INPUT,
         * so the host can feed a line and resume, re-reading the whole line.
         * Only applies before the first byte; a line already in the buffer is
         * read to its break character as usual. */
        if (console && console->char_available && !console->char_available(console->context)) {
            mon_log(MON_LOG_DEBUG, MON_ID_503B ": No input on device %u - suspend (wait for line)",
                    device_no);
            ctx->wait_requested = 1;
            ctx->wait_device = device_no;
            return MON_SUCCESS;  /* not committed; CPU rewinds and retries */
        }

        while (bytes_read < max_bytes) {
            /* Check if we have a console configured */
            if (!console || !console->read_char) {
                break;  /* No console configured - don't block */
            }

            /* Read character - this may block for interactive consoles */
            ch = console->read_char(console->context);

            if (ch == EOF) {
                break;
            }

            buffer[bytes_read++] = (uint8_t)ch;

            /* Echo based on strategy */
            if (should_echo_char_ex((uint8_t)ch, echo_strat, echo_table_ptr, eight_bit_io)) {
                if (console && console->write_char) {
                    console->write_char(console->context, ch);
                } else {
                    putchar(ch);
                }
            }

            /* Check for break character based on strategy */
            if (is_break_char_ex((uint8_t)ch, break_strat, break_table_ptr, eight_bit_io)) {
                break;
            }
        }

        if (!console || !console->write_char) {
            fflush(stdout);
        }

        mon_log(MON_LOG_DEBUG, MON_ID_503B ": Read %u bytes from console (device %u)",
                bytes_read, device_no);
    }
    else if (is_mass_storage_file(device_no)) {
        /* Mass storage file: read from open file table */
        OpenFileEntry* entry = mon_file_table_get((int)device_no);
        if (!entry || !entry->in_use) {
            mon_log(MON_LOG_WARN, MON_ID_503B ": File %u not open", device_no);
            mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
            return MON_ERROR;
        }

        /* Check access mode allows reading */
        if (entry->access_mode == ACCESS_SEQ_WRITE || entry->access_mode == ACCESS_SEQ_APPEND) {
            mon_log(MON_LOG_WARN, MON_ID_503B ": File %u not open for reading", device_no);
            mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
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
                if (is_break_char_ex((uint8_t)ch, break_strat, break_table_ptr, eight_bit_io)) {
                    break;
                }
            }
            mon_log(MON_LOG_DEBUG, MON_ID_503B ": Read %u bytes from file %u, pos=%u",
                    bytes_read, device_no, entry->current_position);
        } else {
            mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
            return MON_ERROR;
        }
    }
    else {
        /* Unsupported device type */
        mon_log(MON_LOG_WARN, MON_ID_503B ": Unsupported device %u", device_no);
        mon_set_error(ctx, MON_ERR_NO_SUCH_DEVICE_NAME);  /* 030B No such device name */
        return MON_ERROR;
    }

    /* Write buffer to emulator memory */
    for (uint32_t i = 0; i < bytes_read; i++) {
        ctx->write_byte(ctx->cpu, buffer_addr + i, buffer[i]);
    }

    /* Return number of bytes read */
    mon_write_param_word(ctx, 2, bytes_read);

    /* Log the string content (sanitized for display) */
    char display_buf[128];
    uint32_t display_len = (bytes_read < 64) ? bytes_read : 64;
    for (uint32_t i = 0; i < display_len; i++) {
        uint8_t c = buffer[i];
        display_buf[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
    }
    display_buf[display_len] = '\0';

    mon_log(MON_LOG_DEBUG, MON_ID_503B ": OUT: Read %u bytes, wrote to 0x%08X",
            bytes_read, buffer_addr);
    mon_log(MON_LOG_DEBUG, MON_ID_503B ":     Content: \"%s\"%s",
            display_buf, (bytes_read > 64) ? "..." : "");

    /* Log hex dump of first 32 bytes */
    if (bytes_read > 0) {
        char hex_buf[128];
        int hex_len = 0;
        uint32_t hex_count = (bytes_read < 32) ? bytes_read : 32;
        for (uint32_t i = 0; i < hex_count; i++) {
            hex_len += snprintf(hex_buf + hex_len, sizeof(hex_buf) - hex_len, "%02X ", buffer[i]);
        }
        mon_log(MON_LOG_DEBUG, MON_ID_503B ":     Hex: %s", hex_buf);
    }

    mon_set_success(ctx);
    return MON_SUCCESS;
}
