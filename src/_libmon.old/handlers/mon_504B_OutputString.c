/*
 * MON 504B [DVOUTS/OutputString]
 *
 * Writes a string to a device, e.g. a terminal or an opened file.
 *
 * - This is the most efficient way to output strings on the ND-500.
 * - The maximum string length is 2048 bytes.
 * - Appendix F contains an ASCII table.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): Logical device number
 *   [I] NoOfBytes (INTEGER): Number of bytes to write (max 2048)
 *   [I] Buffer (STRING): Address of buffer containing string
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"
#include "../mon_device_io.h"
#include <stdio.h>
#include <string.h>

#define DVOUTS_MAX_BYTES 2048

MonResult mon_504B_OutputString(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 3) {
        mon_log(MON_LOG_WARN, MON_ID_504B ": Missing parameters (need 3, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t num_bytes = mon_read_param_word(ctx, 1);
    uint32_t buffer_addr = ctx->arg_addresses[2];

    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "NoOfBytes");

    MonResult r = mon_dvouts_write(ctx, device_no, num_bytes, buffer_addr);
    if (r != MON_SUCCESS) {
        return r;
    }

    mon_set_success(ctx);
    return MON_SUCCESS;
}

/* =========================================================================
 * DVOUTS core - shared with MON 511B DVIO (see mon_device_io.h).
 * ========================================================================= */

MonResult mon_dvouts_write(MonContext* ctx, uint32_t device_no,
                           uint32_t num_bytes, uint32_t buffer_addr) {
    /* Identify device type */
    const char* dev_type = "unknown";
    if (is_character_device(device_no)) dev_type = "character";
    else if (is_terminal(device_no)) dev_type = "terminal";
    else if (is_mass_storage_file(device_no)) dev_type = "file";

    mon_log(MON_LOG_DEBUG, MON_ID_504B ": IN: DeviceNo=%u (%o), NoOfBytes=%u, BufferAddr=0x%08X",
            device_no, device_no, num_bytes, buffer_addr);
    mon_log(MON_LOG_DEBUG, MON_ID_504B ":     DeviceType=%s", dev_type);

    /* Validate byte count */
    if (num_bytes > DVOUTS_MAX_BYTES) {
        mon_log(MON_LOG_WARN, MON_ID_504B ": NoOfBytes %o exceeds max %o", num_bytes, DVOUTS_MAX_BYTES);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    if (num_bytes == 0) {
        /* Nothing to write - caller owns the final status */
        return MON_SUCCESS;
    }

    /* Read string from emulator memory */
    uint8_t buffer[DVOUTS_MAX_BYTES];
    for (uint32_t i = 0; i < num_bytes; i++) {
        buffer[i] = ctx->read_byte(ctx->cpu, buffer_addr + i);
    }

    /* Log the string content (sanitized for display) */
    char display_buf[128];
    uint32_t display_len = (num_bytes < 64) ? num_bytes : 64;
    for (uint32_t i = 0; i < display_len; i++) {
        uint8_t c = buffer[i];
        display_buf[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
    }
    display_buf[display_len] = '\0';

    mon_log(MON_LOG_DEBUG, MON_ID_504B ":     Content: \"%s\"%s",
            display_buf, (num_bytes > 64) ? "..." : "");

    /* Log hex dump of first 32 bytes */
    if (num_bytes > 0) {
        char hex_buf[128];
        int hex_len = 0;
        uint32_t hex_count = (num_bytes < 32) ? num_bytes : 32;
        for (uint32_t i = 0; i < hex_count; i++) {
            hex_len += snprintf(hex_buf + hex_len, sizeof(hex_buf) - hex_len, "%02X ", buffer[i]);
        }
        mon_log(MON_LOG_DEBUG, MON_ID_504B ":     Hex: %s", hex_buf);
    }

    /* Route by device class */
    if (is_character_device(device_no) || is_terminal(device_no)) {
        /* Character device or terminal: use console I/O */
        ConsoleIO* console = mon_file_table_get_console();

        for (uint32_t i = 0; i < num_bytes; i++) {
            if (console && console->write_char) {
                console->write_char(console->context, buffer[i]);
            } else {
                /* Fallback to stdout */
                putchar(buffer[i]);
            }
        }

        if (!console || !console->write_char) {
            fflush(stdout);
        }

        mon_log(MON_LOG_DEBUG, MON_ID_504B ": OUT: Wrote %o bytes to console (device %o)",
                num_bytes, device_no);
    }
    else if (is_mass_storage_file(device_no)) {
        /* Mass storage file: write to open file table */
        OpenFileEntry* entry = mon_file_table_get((int)device_no);
        if (!entry || !entry->in_use) {
            mon_log(MON_LOG_WARN, MON_ID_504B ": File %o not open", device_no);
            mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
            return MON_ERROR;
        }

        /* Check access mode allows writing */
        if (entry->access_mode == ACCESS_SEQ_READ || entry->access_mode == ACCESS_RAND_READ) {
            mon_log(MON_LOG_WARN, MON_ID_504B ": File %o not open for writing", device_no);
            mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
            return MON_ERROR;
        }

        if (entry->host_file) {
            size_t written = fwrite(buffer, 1, num_bytes, entry->host_file);
            if (written != num_bytes) {
                mon_log(MON_LOG_WARN, MON_ID_504B ": Write error on file %o (wrote %zu of %o)",
                        device_no, written, num_bytes);
                mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
                return MON_ERROR;
            }
            /* Flush to ensure data is written to disk */
            fflush(entry->host_file);
            entry->current_position += num_bytes;
            mon_log(MON_LOG_DEBUG, MON_ID_504B ": OUT: Wrote %o bytes to file %o, pos=%o",
                    num_bytes, device_no, entry->current_position);
        } else {
            mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
            return MON_ERROR;
        }
    }
    else {
        /* Unsupported device type */
        mon_log(MON_LOG_WARN, MON_ID_504B ": Unsupported device %o", device_no);
        mon_set_error(ctx, MON_ERR_NO_SUCH_DEVICE_NAME);  /* 030B No such device name */
        return MON_ERROR;
    }

    /* Caller owns the final status (511B still has its input phase to run). */
    return MON_SUCCESS;
}
