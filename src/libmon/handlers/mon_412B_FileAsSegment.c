/*
 * MON 412B [FSCNT/FileAsSegment]
 *
 * Connects a file as a segment to your domain. You can then access the file
 * as a logical segment. This reduces the access time.
 *
 * - The file must be open.
 * - The file is disconnected when it is closed.
 * - A file may be connected to several processes simultaneously.
 * - You may not use ReadFromFile (MON 117) or WriteToFile (MON 120) on a
 *   file which is connected as a segment.
 *
 * Parameters:
 *   [I] FileNo (INTEGER): File number (64-127)
 *   [I] LogSegmentNo (INTEGER): Logical segment number to use
 *   [I] AccessType (INTEGER): Access type (0=read, 1=write, 2=read/write)
 *   [O] SegmentNo (INTEGER): Actual segment number assigned (returned in W1)
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_file_table.h"

MonResult mon_412B_FileAsSegment(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 3) {
        mon_log(MON_LOG_WARN, MON_ID_412B ": Missing parameters (need 3, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t file_no = mon_read_param_word(ctx, 0);
    uint32_t log_segment_no = mon_read_param_word(ctx, 1);
    uint32_t access_type = mon_read_param_word(ctx, 2);

    MON_LOG_IN_WORD(ctx, 0, "FileNo");
    MON_LOG_IN_WORD(ctx, 1, "LogSegmentNo");
    MON_LOG_IN_WORD(ctx, 2, "AccessType");

    mon_log(MON_LOG_DEBUG, MON_ID_412B ": IN: FileNo=%o, LogSegmentNo=%o, AccessType=%o",
            file_no, log_segment_no, access_type);

    /* Validate file number is in mass storage range */
    if (!is_mass_storage_file(file_no)) {
        mon_log(MON_LOG_WARN, MON_ID_412B ": Invalid file number %o (must be 100-177)", file_no);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Validate access type */
    if (access_type > 2) {
        mon_log(MON_LOG_WARN, MON_ID_412B ": Invalid access type %o (must be 0-2)", access_type);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Check file is open */
    OpenFileEntry* entry = mon_file_table_get((int)file_no);
    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, MON_ID_412B ": File %o not open", file_no);
        mon_set_error(ctx, 53);  /* File not open */
        return MON_ERROR;
    }

    /* Check if already mapped */
    if (entry->mapped_as_segment) {
        mon_log(MON_LOG_WARN, MON_ID_412B ": File %o already mapped to segment %o",
                file_no, entry->mapped_segment_no);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Check access mode compatibility */
    if (access_type == 1 || access_type == 2) {  /* Write access requested */
        if (entry->access_mode == ACCESS_SEQ_READ || entry->access_mode == ACCESS_RAND_READ) {
            mon_log(MON_LOG_WARN, MON_ID_412B ": File %o not open for write", file_no);
            mon_set_error(ctx, 52);
            return MON_ERROR;
        }
    }

    /* Mark file as mapped to segment */
    entry->mapped_as_segment = true;
    entry->mapped_segment_no = log_segment_no;
    entry->segment_access_type = (uint8_t)access_type;

    mon_log(MON_LOG_INFO, MON_ID_412B ": OUT: File %o connected as segment %o (access=%o)",
            file_no, log_segment_no, access_type);

    /* Return assigned segment number in W1 */
    ctx->set_error_code(ctx->cpu, log_segment_no);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
