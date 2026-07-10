/*
 * MON 74B (60 decimal): SetStartByte (SETBT)
 *
 * Sets the next byte to be read or written in an opened mass-storage file.
 * 
 * - The bytes in a file are numbered upwards from 0.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): input
 *   [I] BytePointer (INTEGER4): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"
#include <stdio.h>

MonResult mon_74B_SetStartByte(MonContext* ctx) {
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_74B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* FileNumber [I], BytePointer [I] - both 32-bit words on ND-500 */
    uint32_t file_no = mon_read_param_word(ctx, 0);
    uint32_t byte_ptr = mon_read_param_word(ctx, 1);

    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "BytePointer");

    if (!is_mass_storage_file(file_no)) {
        mon_log(MON_LOG_WARN, MON_ID_74B ": Invalid file number %o", file_no);
        mon_set_error(ctx, MON_ERR_FILE_NUMBER_RANGE);  /* 127B File number out of range */
        return MON_ERROR;
    }

    OpenFileEntry* entry = mon_file_table_get((int)file_no);
    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, MON_ID_74B ": File %o not open", file_no);
        mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
        return MON_ERROR;
    }

    /* Set the sequential read/write position to the given byte (first byte
     * is 0). Track it in the entry and seek the host file if present. */
    entry->current_position = byte_ptr;
    if (entry->host_file) {
        if (fseek(entry->host_file, (long)byte_ptr, SEEK_SET) != 0) {
            mon_log(MON_LOG_WARN, MON_ID_74B ": Seek to byte %u failed on file %o",
                    byte_ptr, file_no);
            mon_set_error(ctx, MON_ERR_NO_SUCH_BLOCK);  /* 143B No such block */
            return MON_ERROR;
        }
    }

    mon_log(MON_LOG_DEBUG, MON_ID_74B ": File %o byte pointer set to %u", file_no, byte_ptr);
    mon_set_success(ctx);
    return MON_SUCCESS;
}
