/*
 * MON 7B (7 decimal): ReadBlock (RPAGE)
 *
 * Reads randomly from a file. You read one block at a time. The file must be opened for random read access.
 * 
 * - The standard block size is 512 bytes. You can change this with SetBlockSize. The first block is number 0.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER2): input
 *   [I] BlockNo (INTEGER2): input
 *   [O] DataDestination (ARRAY): output
 *   [O] ErrCode (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_7B_ReadBlock(MonContext* ctx) {
    /* TODO: Implement ReadBlock (RPAGE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "BlockNo");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
