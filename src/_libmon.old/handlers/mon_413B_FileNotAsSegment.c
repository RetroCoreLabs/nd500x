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
 *   [I] LogSegmentNumber (INTEGER): Logical segment number to disconnect.
 *       OPTIONAL. If omitted, the file is disconnected from whichever segment
 *       it is currently mapped to.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"

MonResult mon_413B_FileNotAsSegment(MonContext* ctx) {
    /* LogSegmentNumber is an OPTIONAL parameter, so one argument is legal.
     * Only FileNumber is mandatory. */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, MON_ID_413B ": Missing parameters (need at least 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t file_no = mon_read_param_word(ctx, 0);
    bool have_segment_no = (ctx->arg_count >= 2);
    uint32_t log_segment_no = have_segment_no ? mon_read_param_word(ctx, 1) : 0;

    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    if (have_segment_no) {
        MON_LOG_IN_WORD(ctx, 1, "LogSegmentNumber");
        mon_log(MON_LOG_DEBUG, MON_ID_413B ": IN: FileNo=%o, LogSegmentNo=%o",
                file_no, log_segment_no);
    } else {
        mon_log(MON_LOG_DEBUG, MON_ID_413B ": IN: FileNo=%o, LogSegmentNo omitted",
                file_no);
    }

    /* Validate file number is in mass storage range */
    if (!is_mass_storage_file(file_no)) {
        mon_log(MON_LOG_WARN, MON_ID_413B ": Invalid file number %o (must be 100-177)", file_no);
        mon_set_error(ctx, MON_ERR_FILE_NUMBER_RANGE);  /* 127B File number out of range */
        return MON_ERROR;
    }

    /* Check file is open (file should still be open when disconnecting segment) */
    OpenFileEntry* entry = mon_file_table_get((int)file_no);
    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, MON_ID_413B ": File %o not open", file_no);
        mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
        return MON_ERROR;
    }

    /* Check if this file is actually mapped as a segment */
    if (!entry->mapped_as_segment) {
        mon_log(MON_LOG_WARN, MON_ID_413B ": File %o not mapped as segment", file_no);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    /* If the caller named a segment, it must be the one the file is mapped to.
     * If the caller omitted it, disconnect whichever segment that is. */
    if (have_segment_no && entry->mapped_segment_no != log_segment_no) {
        mon_log(MON_LOG_WARN, MON_ID_413B ": File %o mapped to segment %o, not %o",
                file_no, entry->mapped_segment_no, log_segment_no);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    uint32_t disconnected_from = entry->mapped_segment_no;

    /* Flush the segment back to the host file BEFORE dropping the mapping.
     * A connected file is MAPPED, not copied: the program reads and writes the
     * file through ordinary memory accesses to the segment, so everything it
     * built there exists only in the segment's pages until now. The ND Linker
     * constructs an entire :DOM this way and never issues a 120B WFILE for the
     * domain body - without this flush a linked domain was written out as
     * megabytes of zeros with a null start address, and would not run. */
    if (ctx->writeback_file_segment && entry->host_path[0]) {
        int wrc = ctx->writeback_file_segment(ctx->cpu, 0xFF /* CED */,
                                              disconnected_from, entry->host_path);
        if (wrc < 0) {
            mon_log(MON_LOG_WARN, MON_ID_413B ": write-back of segment %o to '%s' FAILED",
                    disconnected_from, entry->host_path);
        } else if (wrc > 0) {
            mon_log(MON_LOG_INFO, MON_ID_413B ": segment %o written back to '%s'",
                    disconnected_from, entry->host_path);
        }   /* wrc == 0: read-only mapping, nothing to write - say nothing */
    }
    if (ctx->release_file_segment) {
        ctx->release_file_segment(0xFF /* CED */, disconnected_from);
    }

    /* Clear segment mapping state. The file itself stays open. */
    entry->mapped_as_segment = false;
    entry->mapped_segment_no = 0;
    entry->segment_access_type = 0;

    mon_log(MON_LOG_INFO, MON_ID_413B ": OUT: File %o disconnected from segment %o",
            file_no, disconnected_from);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
