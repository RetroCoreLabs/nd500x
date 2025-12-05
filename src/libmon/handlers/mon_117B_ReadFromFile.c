/*
 * MON 117B (79 decimal): ReadFromFile (RFILE)
 *
 * Reads any number of bytes from a file. The read operation must start at the beginning of a block. The file must be opened for random read access.
 * 
 * - The standard block size is 512 bytes. You can change this with SetBlockSize. The first block is number 0.
 * - You may use access code D for direct transfer. Then the block size must be a multiple of the page size. The number of bytes to transfer must be a multiple of the block size.
 * - Peripheral files are always read sequentially.
 * - Data transfer across segment or RT common limits is illegal as this would be likely to result in inconsistent data.
 *
 * Parameters:
 *   [I] FileNo (INTEGER2): input
 *   [I] WaitFlag (INTEGER2): input
 *   [O] Buff (BYTES): output
 *   [I] BlockNo (INTEGER2): input
 *   [I] NoOfBytes (LONGINT): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_117B_ReadFromFile(MonContext* ctx) {
    /* TODO: Implement ReadFromFile (RFILE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNo");
    MON_LOG_IN_WORD(ctx, 1, "WaitFlag");
    MON_LOG_IN_WORD(ctx, 3, "BlockNo");
    MON_LOG_IN_WORD(ctx, 4, "NoOfBytes");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
