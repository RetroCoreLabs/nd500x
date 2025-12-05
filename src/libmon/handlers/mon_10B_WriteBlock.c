/*
 * MON 10B (8 decimal): WriteBlock (WPAGE)
 *
 * Writes randomly to a file. You write one block at a time. The file must be opened for random write access.
 * 
 * - The standard block size is 512 bytes. You can change this with SetBlockSize. The first block is number 0.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER2): input
 *   [I] BlockNumber (INTEGER2): input
 *   [I] Buffer (ARRAY): input
 *   [O] ErrCode (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_10B_WriteBlock(MonContext* ctx) {
    /* TODO: Implement WriteBlock (WPAGE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "BlockNumber");
    MON_LOG_IN_WORD(ctx, 2, "Buffer");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
