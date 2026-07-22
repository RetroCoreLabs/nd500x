/*
 * MON 144B (100 decimal): DeviceFunction (MAGTP)
 *
 * Performs various operations on floppy disks, magnetic tapes, Versatec plotters, and SCSI streamers.
 *
 * - The parameter values depend on the device.
 * - If the function code is in the range 5B to 24B, except 23B, the parameter buffer and the two device dependent parameters are dummies.
 * - If the function code is in the range 20B to 24B, except 23B, the hardware status is returned.
 *
 * Parameters:
 *   [I] FunctionCode (INTEGER2): input
 *   [IO] Buffer (INTEGER2[1024]): in/out
 *   [I] DeviceNo (INTEGER2): input
 *   [I] DeviceParam1 (INTEGER2): input
 *   [I] DeviceParam2 (INTEGER2): input
 *
 * FUNCTION CODES - primary source: ND-60.050.06 SINTRAN III Users Guide,
 * Table 9.1, page 232. Codes are OCTAL:
 *
 *   0   Read-Record        param1 = octal addr of data buffer (area users only)
 *                          param2 = octal no. of words
 *   1   Write-Record       (as above)
 *   5   Unlock-and-Stop    6  Lock-Cassette      7   Erase-EOF
 *   10  Advance-to-EOF     11 Reverse-to-EOF     12  Write-EOF
 *   13  Rewind             14 Write-Erase-Gap    15  Back-Space-Records
 *   16  Advance-Records    17 Unload
 *   20  Read Status        21 Clear Device       23  Select Density-and-Parity
 *   24  Read Last Status
 *
 * Corroboration that this is the right table: the ND linker's own wrapper at
 * 0xB004E98A special-cases function codes 0x10 and 0x14 (16 and 20 decimal =
 * 20B and 24B octal) = exactly Read Status / Read Last Status, the codes the
 * manual says take dummy parameters. The table's numbering skips 8 and 9,
 * confirming it is octal.
 */

#include "../mon.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"
#include <stdlib.h>
#include <stdio.h>

/* Function codes (octal in the manual, written here as C literals). */
#define MAGTP_FUNC_READ_RECORD   0    /* 0B  */
#define MAGTP_FUNC_WRITE_RECORD  1    /* 1B  */
#define MAGTP_FUNC_READ_STATUS   16   /* 20B */
#define MAGTP_FUNC_READ_LAST_ST  20   /* 24B */

/*
 * Function 0B - Read-Record.
 *
 * The ND linker uses this to load its DDB tables: it OPENs 'DDBTABLES-G':VTM,
 * SETBTs the byte pointer to 0, then issues MAGTP(0, buffer, <that file>, ...)
 * and immediately requires the first word of the buffer to be 1
 * (0xB004AFB8: "w comp2 r.0x0,$0x1", else error 0x106A). The real
 * DDBTABLES-G06:VTM begins with 0x00000001 and carries 0x40-byte records at
 * +0x78, which is exactly where the linker indexes - so the file image maps
 * directly onto the buffer.
 */
static MonResult magtp_read_record(MonContext* ctx, uint32_t device_no,
                                   uint32_t buffer_addr, uint32_t num_bytes) {
    OpenFileEntry* entry = mon_file_table_get((int)device_no);
    if (!entry || !entry->host_file) {
        mon_log(MON_LOG_WARN, "MON 144B MAGTP: Read-Record on device %o which is not an open file",
                device_no);
        mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B */
        return MON_ERROR;
    }

    if (num_bytes == 0) {
        mon_set_success(ctx);
        return MON_SUCCESS;
    }

    /* Resume at the position the caller established (the linker sets it with
     * 74B SETBT before calling), matching how 117B RFILE streams. */
    if (fseek(entry->host_file, (long)entry->current_position, SEEK_SET) != 0) {
        mon_log(MON_LOG_WARN, "MON 144B MAGTP: Seek to %o failed on device %o",
                entry->current_position, device_no);
        mon_set_error(ctx, MON_ERR_END_OF_FILE);  /* 003B */
        return MON_ERROR;
    }

    uint8_t* buffer = (uint8_t*)malloc(num_bytes);
    if (!buffer) {
        mon_set_error(ctx, MON_ERR_NO_BUFFER_SPACE);  /* 131B */
        return MON_ERROR;
    }

    size_t bytes_read = fread(buffer, 1, num_bytes, entry->host_file);
    if (bytes_read == 0 && ferror(entry->host_file)) {
        mon_log(MON_LOG_WARN, "MON 144B MAGTP: Read error on device %o", device_no);
        clearerr(entry->host_file);
        free(buffer);
        mon_set_error(ctx, MON_ERR_TRANSFER_ERROR);  /* 141B */
        return MON_ERROR;
    }

    for (size_t i = 0; i < bytes_read; i++)
        ctx->write_byte(ctx->cpu, buffer_addr + (uint32_t)i, buffer[i]);

    entry->current_position = (uint32_t)ftell(entry->host_file);
    free(buffer);

    mon_log(MON_LOG_DEBUG,
            "MON 144B MAGTP: Read-Record %zu bytes from device %o into 0x%08X, pos=%o",
            bytes_read, device_no, buffer_addr, entry->current_position);

    mon_set_success(ctx);
    return MON_SUCCESS;
}

MonResult mon_144B_DeviceFunction(MonContext* ctx) {
    if (ctx->arg_count < 5) {
        mon_log(MON_LOG_WARN, "MON 144B MAGTP: Missing parameters (need 5, got %u)", ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B */
        return MON_ERROR;
    }

    uint32_t func         = mon_read_param_word(ctx, 0);
    uint32_t buffer_addr  = ctx->arg_addresses[1];
    uint32_t device_no    = mon_read_param_word(ctx, 2);
    uint32_t device_prm1  = mon_read_param_word(ctx, 3);
    uint32_t device_prm2  = mon_read_param_word(ctx, 4);

    MON_LOG_IN_WORD(ctx, 0, "FunctionCode");
    MON_LOG_IN_WORD(ctx, 2, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 3, "DeviceParam1");
    MON_LOG_IN_WORD(ctx, 4, "DeviceParam2");

    mon_log(MON_LOG_DEBUG,
            "MON 144B MAGTP: IN: Func=%o, BuffAddr=0x%08X, DeviceNo=%o, Param1=%o, Param2=%o",
            func, buffer_addr, device_no, device_prm1, device_prm2);

    switch (func) {
    case MAGTP_FUNC_READ_RECORD:
        /* UNVERIFIED: which parameter carries the transfer size. Table 9.1 lists
         * param2 as "octal no. of words", but the linker passes param1=4096 and
         * param2=3, and 4096 is the only value that can span the table it then
         * indexes (count at +0x0, 0x40-byte records from +0x78). The manual's
         * "following pages" that would settle the ND-500 parameter convention
         * are not in the scanned set. Treating param1 as a BYTE count is
         * therefore INFERRED - it is what the linker's own usage demands, not
         * something read from a manual. Revisit if another caller disagrees. */
        return magtp_read_record(ctx, device_no, buffer_addr, device_prm1);

    default:
        /* Every other function code is a real device operation (tape motion,
         * formatting, hardware status) against hardware we do not emulate.
         * Returning benign SUCCESS keeps callers moving; it is NOT correct, and
         * the true return convention (status word / IO buffer format) is
         * UNVERIFIED. Do not port as final. */
        mon_log(MON_LOG_WARN, "MON 144B MAGTP: Function %o not implemented - returning benign SUCCESS",
                func);
        mon_set_success(ctx);
        return MON_SUCCESS;
    }
}
