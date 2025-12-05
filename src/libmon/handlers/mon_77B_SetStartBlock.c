/*
 * MON 77B (63 decimal): SetStartBlock (SETBL)
 *
 * Sets the next block to be read or written in an opened file. You may access the first bytes in the block with the monitor calls to read and write bytes.
 * 
 * - The standard block size is 512 bytes. This block size is set when the file is opened. You can change this with SetBlockSize.
 * - The first block of a file is number 0.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): input
 *   [I] BlockNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_77B_SetStartBlock(MonContext* ctx) {
    /* TODO: Implement SetStartBlock (SETBL) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "BlockNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
