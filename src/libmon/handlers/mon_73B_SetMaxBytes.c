/*
 * MON 73B [SMAX/SetMaxBytes]
 *
 * Sets the value of the maximum byte pointer in an opened file (i.e. the
 * number of bytes minus 1). The specified number of bytes are stored when
 * the file is closed. Error code 3 is returned if you later try to read
 * beyond this size.
 *
 * - The file must be opened for write.
 * - This monitor call is only relevant for sequential access.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): File number (64-127 for mass storage)
 *   [I] MaxBytePointer (INTEGER4): Maximum byte pointer value
 *
 * Returns:
 *   K flag set on error, error code in W1:
 *     52 = Invalid parameter
 *     53 = File not open
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

MonResult mon_73B_SetMaxBytes(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_73B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t file_no = mon_read_param_word(ctx, 0);
    uint32_t max_byte_ptr = mon_read_param_word(ctx, 1);

    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "MaxBytePointer");

    mon_log(MON_LOG_DEBUG, MON_ID_73B ": IN: FileNumber=%o, MaxBytePointer=%o",
            file_no, max_byte_ptr);

    /* Validate file number is in mass storage range */
    if (!is_mass_storage_file(file_no)) {
        mon_log(MON_LOG_WARN, MON_ID_73B ": Invalid file number %o (must be %o-%o)", file_no, 64, 127);
        mon_set_error(ctx, MON_ERR_FILE_NUMBER_RANGE);  /* 127B File number out of range */
        return MON_ERROR;
    }

    /* Look up file in open file table */
    OpenFileEntry* entry = mon_file_table_get((int)file_no);
    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, MON_ID_73B ": File %o not open", file_no);
        mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
        return MON_ERROR;
    }

    /* Check access mode allows writing */
    if (entry->access_mode == ACCESS_SEQ_READ || entry->access_mode == ACCESS_RAND_READ) {
        mon_log(MON_LOG_WARN, MON_ID_73B ": File %o not open for write", file_no);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    /* Set max bytes (pointer + 1 = number of bytes) */
    uint32_t new_size = max_byte_ptr + 1;
    entry->object_entry.bytes_in_file = new_size;

    /* If we have a host file, truncate or extend it */
    if (entry->host_file) {
        /* Truncate file to new size */
        int fd = fileno(entry->host_file);
        if (fd >= 0) {
            fflush(entry->host_file);
            /* Use platform-appropriate truncate - ftruncate on POSIX */
            #ifdef _WIN32
            _chsize(fd, (long)new_size);
            #else
            ftruncate(fd, (off_t)new_size);
            #endif
        }
    }

    mon_log(MON_LOG_DEBUG, MON_ID_73B ": OUT: File %o max bytes set to %o", file_no, new_size);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
