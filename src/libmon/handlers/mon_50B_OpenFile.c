/*
 * MON 50B [OPEN/OpenFile]
 *
 * Opens a file for access. You must open a file before reading or writing to it.
 * The access mode determines what operations are allowed.
 *
 * Parameters (from MASTER reference ND-860228.2 EN):
 *   [IO] FileNo (INTEGER): If 0 on input, returns the ND-500 open file number.
 *                          Otherwise specifies the file number to use.
 *   [I]  AccessCode (INTEGER): Access code specifying type of file access:
 *        0 = Sequential write
 *        1 = Sequential read
 *        2 = Random read or write
 *        3 = Random read only
 *        4 = Sequential read or write
 *        5 = Sequential write append
 *        6 = Random read or write common on contiguous files
 *        7 = Random read common on contiguous files
 *        8 = Random read or write on contiguous files (direct transfer for RT)
 *        9 = Random read, write append for WriteToFile
 *   [I]  FileName (STRING): File name string (up to 64 characters, 0x27 terminated)
 *   [I]  FileType (STRING): Default file type string (up to 4 characters, 0x27 terminated)
 *
 * Returns:
 *   File number in W1 register on success
 *   K flag set on error, error code in W1:
 *     46 = File not found
 *     52 = Invalid parameter
 *     55 = No free file slots
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_file_table.h"

MonResult mon_50B_OpenFile(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 4) {
        mon_log(MON_LOG_WARN, MON_ID_50B ": Missing parameters (need 4, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Parameter order per MASTER reference (ND-860228.2 EN):
     * [0] FileNo (IO) - If 0, returns allocated file number; otherwise use specified
     * [1] AccessCode (I)
     * [2] FileName (STRING)
     * [3] FileType (STRING)
     */

    /* Read FileNo input - if 0, allocate new; otherwise use specified */
    int32_t file_no_input = (int32_t)mon_read_param_word(ctx, 0);

    /* Read access code */
    uint32_t access_code = mon_read_param_word(ctx, 1);

    /* Read filename string from memory (up to 64 chars per MASTER reference) */
    char filename[65];
    mon_read_string(ctx, 2, filename, 65);

    /* Read file type string (up to 4 chars) */
    char filetype[5];
    mon_read_string(ctx, 3, filetype, 5);

    MON_LOG_IN_WORD(ctx, 0, "FileNo");
    MON_LOG_IN_WORD(ctx, 1, "AccessCode");

    mon_log(MON_LOG_DEBUG, MON_ID_50B ": IN: FileNo=%d, AccessCode=%o, FileName='%s', FileType='%s'",
            file_no_input, access_code, filename, filetype);

    /* Validate access code */
    if (access_code > 9) {
        mon_log(MON_LOG_WARN, MON_ID_50B ": Invalid access code %o", access_code);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Open file via file table API
     * Per MASTER reference: If FileNo is 0, allocate new; otherwise use specified number */
    int file_number = mon_file_open_ex(filename, filetype, (uint8_t)access_code, file_no_input);

    if (file_number < 0) {
        mon_log(MON_LOG_WARN, MON_ID_50B ": Failed to open '%s.%s' (error %d)",
                filename, filetype, file_number);

        /* Map internal error codes to SINTRAN error codes */
        switch (file_number) {
            case -52: mon_set_error(ctx, 52); break;  /* Invalid parameter */
            case -54: mon_set_error(ctx, 54); break;  /* File already open */
            case -55: mon_set_error(ctx, 55); break;  /* No free file slots */
            default:  mon_set_error(ctx, 46); break;  /* No such filename */
        }
        return MON_ERROR;
    }

    mon_log(MON_LOG_INFO, MON_ID_50B ": OUT: Opened '%s.%s' as file %o (access=%o)",
            filename, filetype, file_number, access_code);

    /* Return file number in W1 (I1) register */
    if (ctx->set_error_code) {
        ctx->set_error_code(ctx->cpu, (uint32_t)file_number);
    }

    /* Also write to FileNo output parameter if provided */
    mon_write_param_word(ctx, 0, (uint32_t)file_number);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
