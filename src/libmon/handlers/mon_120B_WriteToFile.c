/*
 * MON 120B (80 decimal): WriteToFile (WFILE)
 *
 * Writes any number of bytes to a file. The read operation must start at the beginning of a block. The file must be opened for random write access.
 * 
 * - The standard block size is 512 bytes. You can change this with SetBlockSize. The first block is number 0.
 * - You may use access code D for direct transfer. Then the block size must be a multiple of the page size. The number of bytes to transfer must be a multiple of the block size. The data must be fixed contiguously in memory.
 * - Peripheral files are always written to sequentially.
 * - Data transfer across segment or RT common limits is illegal as this would be likely to result in inconsistent data.
 *
 * Parameters:
 *   [I] FileNo (INTEGER2): input
 *   [I] ReturnFlag (INTEGER2): input
 *   [I] Buff (BYTES): input
 *   [I] BlockNo (INTEGER2): input
 *   [I] NoOfBytes (LONGINT): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_120B_WriteToFile(MonContext* ctx) {
    /* TODO: Implement WriteToFile (WFILE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNo");
    MON_LOG_IN_WORD(ctx, 1, "ReturnFlag");
    MON_LOG_IN_WORD(ctx, 2, "Buff");
    MON_LOG_IN_WORD(ctx, 3, "BlockNo");
    MON_LOG_IN_WORD(ctx, 4, "NoOfBytes");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
