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
 *   [I] LogSegmentNo (INTEGER): Logical segment number to use (0 = first free)
 *   [I] AccessType (INTEGER): 0 = file contains initial data, 1 = uninitialized/
 *                             empty, 2 = primarily sequential, 3 = combination
 *                             of 1 and 2. (NOT read/write/rdwr - the segment's
 *                             read/write capability comes from the file's OPEN
 *                             mode.) Corrected per 412B_FileAsSegment.yaml + carve.
 *   [O] SegmentNo (INTEGER): Actual segment number assigned (returned in W1)
 *
 * Now REAL: maps the open file's bytes into an MMU/PST-backed logical segment in
 * the caller's (CED) domain via ctx->connect_file_as_segment. Unblocks CAT-500's
 * scratch-as-segment reads (the C compiler back-end). Handoff:
 * docs/HANDOFF_412B_FSCNT_FileAsSegment.md.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"

MonResult mon_412B_FileAsSegment(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 3) {
        mon_log(MON_LOG_WARN, MON_ID_412B ": Missing parameters (need 3, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
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
        mon_set_error(ctx, MON_ERR_FILE_NUMBER_RANGE);  /* 127B File number out of range */
        return MON_ERROR;
    }

    /* Validate access type. Per the YAML/carve, AccessType is
     *   0 = file contains initial data, 1 = uninitialized/empty,
     *   2 = primarily sequential, 3 = combination of 1 and 2.
     * (NOT read/write/rdwr - the read/write capability comes from the file's
     * OPEN mode, below.) */
    if (access_type > 3) {
        mon_log(MON_LOG_WARN, MON_ID_412B ": Invalid access type %o (must be 0-3)", access_type);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    /* Check file is open */
    OpenFileEntry* entry = mon_file_table_get((int)file_no);
    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, MON_ID_412B ": File %o not open", file_no);
        mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
        return MON_ERROR;
    }

    /* Check if already mapped */
    if (entry->mapped_as_segment) {
        mon_log(MON_LOG_WARN, MON_ID_412B ": File %o already mapped to segment %o",
                file_no, entry->mapped_segment_no);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    /* Capability access mode comes from the FILE'S OPEN MODE, not AccessType.
     * Read-only open modes -> RO segment; everything else -> RW. */
    int writable = !(entry->access_mode == ACCESS_SEQ_READ ||
                     entry->access_mode == ACCESS_RAND_READ ||
                     entry->access_mode == ACCESS_RAND_READ_CTG);

    if (!ctx->connect_file_as_segment) {
        mon_log(MON_LOG_WARN, MON_ID_412B ": no connect_file_as_segment callback");
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);
        return MON_ERROR;
    }

    /* Ensure the file's bytes are on disk before we map them. */
    if (entry->host_file) fflush(entry->host_file);

    uint32_t assigned = 0;
    int rc = ctx->connect_file_as_segment(ctx->cpu, ctx->machine,
                 /*domain=*/ 0xFF /* callback resolves to CED */,
                 log_segment_no, access_type, writable,
                 entry->host_path, entry->object_entry.bytes_in_file, &assigned);
    if (rc != 0) {
        mon_log(MON_LOG_WARN, MON_ID_412B ": connect failed rc=%d for '%s'", rc, entry->host_path);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    entry->mapped_as_segment = true;
    entry->mapped_segment_no = assigned;
    entry->segment_access_type = (uint8_t)access_type;

    mon_log(MON_LOG_INFO, MON_ID_412B ": OUT: File %o connected as segment %o "
            "(accessType=%o, writable=%d, bytes=%u)",
            file_no, assigned, access_type, writable, entry->object_entry.bytes_in_file);

    /* Return assigned segment number in W1 */
    ctx->set_error_code(ctx->cpu, (int32_t)assigned);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
