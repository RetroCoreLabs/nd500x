/*
 * MON 6B (6 decimal): WriteScratchFile (WDISK)
 *
 * Writes randomly to the scratch file. One block is transferred. There is one scratch file connected to each terminal. It is opened for random read and write access when you log in. Its file number is 100B.
 *
 * Parameters:
 *   [I] BlockNumber (INTEGER): input
 *   [I] Buffer (ARRAY): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_6B_WriteScratchFile(MonContext* ctx) {
    /* TODO: Implement WriteScratchFile (WDISK) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "BlockNumber");
    MON_LOG_IN_WORD(ctx, 1, "Buffer");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
