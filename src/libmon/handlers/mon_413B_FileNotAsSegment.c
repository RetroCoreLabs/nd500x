/*
 * MON 413B [FSCDNT/FileNotAsSegment]
 *
 * Disconnects a file as a segment in your domain. FileAsSegment allows files
 * to be accessed as segments. This monitor call disconnects the file.
 *
 * - The file is not closed.
 * - The file is automatically disconnected by CloseFile.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): File number (64-127)
 *   [I] LogSegmentNumber (INTEGER): Logical segment number to disconnect
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_file_table.h"

MonResult mon_413B_FileNotAsSegment(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_413B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t file_no = mon_read_param_word(ctx, 0);
    uint32_t log_segment_no = mon_read_param_word(ctx, 1);

    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "LogSegmentNumber");

    mon_log(MON_LOG_DEBUG, MON_ID_413B ": IN: FileNo=%o, LogSegmentNo=%o",
            file_no, log_segment_no);

    /* Validate file number is in mass storage range */
    if (!is_mass_storage_file(file_no)) {
        mon_log(MON_LOG_WARN, MON_ID_413B ": Invalid file number %o (must be 100-177)", file_no);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Check file is open (file should still be open when disconnecting segment) */
    OpenFileEntry* entry = mon_file_table_get((int)file_no);
    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, MON_ID_413B ": File %o not open", file_no);
        mon_set_error(ctx, 53);  /* File not open */
        return MON_ERROR;
    }

    /* Check if this file is actually mapped as a segment */
    if (!entry->mapped_as_segment) {
        mon_log(MON_LOG_WARN, MON_ID_413B ": File %o not mapped as segment", file_no);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Check if segment number matches */
    if (entry->mapped_segment_no != log_segment_no) {
        mon_log(MON_LOG_WARN, MON_ID_413B ": File %o mapped to segment %o, not %o",
                file_no, entry->mapped_segment_no, log_segment_no);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Clear segment mapping state */
    entry->mapped_as_segment = false;
    entry->mapped_segment_no = 0;
    entry->segment_access_type = 0;

    mon_log(MON_LOG_INFO, MON_ID_413B ": OUT: File %o disconnected from segment %o",
            file_no, log_segment_no);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
