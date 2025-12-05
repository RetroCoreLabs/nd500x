/*
 * MON 76B (62 decimal): SetBlockSize (SETBS)
 *
 * Sets the block size of an opened file. Monitor calls which read randomly from, or write randomly to a file, operate on blocks. See ReadFromFile and WriteToFile.
 * 
 * - The standard block size is 512 bytes. This block size is set when the file is opened.
 * - The block size is reset when the file is closed.
 * - Factors of 2048 bytes are the most efficient block sizes.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER2): input
 *   [I] BlockSize (LONGINT): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_76B_SetBlockSize(MonContext* ctx) {
    /* TODO: Implement SetBlockSize (SETBS) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "BlockSize");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
