/*
 * MON 43B [CLOSE/CloseFile]
 *
 * Closes a file that was opened with OpenFile (MON 50B).
 *
 * - CloseFile also resets peripheral files (similar to DeviceControl with -1).
 * - For non-RT programs, files are closed when your program terminates.
 * - Releases any reservation on the file.
 *
 * Parameters:
 *   [I] FileNumber (WORD): File number to close (64-127 for mass storage)
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

MonResult mon_43B_CloseFile(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, MON_ID_43B ": Missing parameters (need 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read file number as SIGNED - negative values have special meaning */
    int32_t file_number = (int32_t)mon_read_param_word(ctx, 0);

    MON_LOG_IN_WORD(ctx, 0, "FileNumber");

    mon_log(MON_LOG_DEBUG, MON_ID_43B ": IN: FileNumber=%d", file_number);

    /* Handle special case: -1 means close all files not permanently open */
    if (file_number == -1) {
        mon_log(MON_LOG_INFO, MON_ID_43B ": Closing all files (FileNumber=-1)");
        mon_file_table_reset();
        mon_set_success(ctx);
        return MON_SUCCESS;
    }

    /* Handle special case: -2 means close all including scratch and permanently open */
    if (file_number == -2) {
        mon_log(MON_LOG_INFO, MON_ID_43B ": Closing all files including scratch (FileNumber=-2)");
        mon_file_table_reset();
        mon_set_success(ctx);
        return MON_SUCCESS;
    }

    /* Validate file number range for normal close */
    if (!mon_file_table_is_valid_file_number(file_number)) {
        mon_log(MON_LOG_WARN, MON_ID_43B ": Invalid file number %d (must be %o-%o)",
                file_number, 64, 127);
        mon_set_error(ctx, MON_ERR_FILE_NUMBER_RANGE);  /* 127B File number out of range */
        return MON_ERROR;
    }

    /* Close file via file table API */
    int result = mon_file_close(file_number);

    if (result < 0) {
        mon_log(MON_LOG_WARN, MON_ID_43B ": File %o not open", file_number);
        mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
        return MON_ERROR;
    }

    mon_log(MON_LOG_INFO, MON_ID_43B ": OUT: Closed file %o", file_number);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
