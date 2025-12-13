/*
 * MON 62B (50 decimal): GetBytesInFile (RMAX)
 *
 * Gets the number of bytes in a file. Only the bytes containing data are counted.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): File number (64-127 for mass storage)
 *   [O] NoOfBytes (INTEGER4): Number of bytes in file (returned in W1)
 *
 * Returns:
 *   K flag set on error, error code in W1:
 *     52 = Invalid parameter
 *     53 = File not open
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_file_table.h"
#include <stdio.h>

MonResult mon_62B_GetBytesInFile(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, "MON 62B RMAX: Missing parameters (need 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read file number */
    uint32_t file_no = mon_read_param_word(ctx, 0);

    MON_LOG_IN_WORD(ctx, 0, "FileNumber");

    mon_log(MON_LOG_DEBUG, "MON 62B RMAX: FileNumber=%u", file_no);

    /* Validate file number is in mass storage range */
    if (!is_mass_storage_file(file_no)) {
        mon_log(MON_LOG_WARN, "MON 62B RMAX: Invalid file number %u (must be 64-127)", file_no);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Look up file in open file table */
    OpenFileEntry* entry = mon_file_table_get((int)file_no);
    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, "MON 62B RMAX: File %u not open", file_no);
        mon_set_error(ctx, 53);  /* File not open */
        return MON_ERROR;
    }

    /* Get file size from object entry */
    uint32_t bytes_in_file = entry->object_entry.bytes_in_file;

    /* If we have a host file, get actual size */
    if (entry->host_file) {
        long current_pos = ftell(entry->host_file);
        if (fseek(entry->host_file, 0, SEEK_END) == 0) {
            bytes_in_file = (uint32_t)ftell(entry->host_file);
            fseek(entry->host_file, current_pos, SEEK_SET);
        }
    }

    mon_log(MON_LOG_DEBUG, "MON 62B RMAX: File %u has %u bytes", file_no, bytes_in_file);

    /* Return bytes in W1 */
    ctx->set_error_code(ctx->cpu, bytes_in_file);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
