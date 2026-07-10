/*
 * MON 76B [SETBS/SetBlockSize]
 *
 * Sets the block size of an opened file. Monitor calls which read randomly
 * from, or write randomly to a file, operate on blocks. See ReadFromFile
 * and WriteToFile.
 *
 * - The standard block size is 512 bytes. This block size is set when the
 *   file is opened.
 * - The block size is reset when the file is closed.
 * - Factors of 2048 bytes are the most efficient block sizes.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): File number (64-127 for mass storage)
 *   [I] BlockSize (LONGINT): Block size in bytes
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

/* Maximum reasonable block size */
#define MAX_BLOCK_SIZE 65536

MonResult mon_76B_SetBlockSize(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_76B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t file_no = mon_read_param_word(ctx, 0);
    uint32_t block_size = mon_read_param_word(ctx, 1);

    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "BlockSize");

    mon_log(MON_LOG_DEBUG, MON_ID_76B ": IN: FileNumber=%o, BlockSize=%o",
            file_no, block_size);

    /* Validate file number is in mass storage range */
    if (!is_mass_storage_file(file_no)) {
        mon_log(MON_LOG_WARN, MON_ID_76B ": Invalid file number %o (must be %o-%o)", file_no, 64, 127);
        mon_set_error(ctx, MON_ERR_FILE_NUMBER_RANGE);  /* 127B File number out of range */
        return MON_ERROR;
    }

    /* Look up file in open file table */
    OpenFileEntry* entry = mon_file_table_get((int)file_no);
    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, MON_ID_76B ": File %o not open", file_no);
        mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
        return MON_ERROR;
    }

    /* Validate block size */
    if (block_size == 0 || block_size > MAX_BLOCK_SIZE) {
        mon_log(MON_LOG_WARN, MON_ID_76B ": Invalid block size %o (must be 1-%o)",
                block_size, MAX_BLOCK_SIZE);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    /* Set block size */
    entry->block_size = block_size;

    mon_log(MON_LOG_DEBUG, MON_ID_76B ": OUT: File %o block size set to %o", file_no, block_size);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
