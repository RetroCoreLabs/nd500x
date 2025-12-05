/*
 * MON 5B (5 decimal): ReadScratchFile (RDISK)
 *
 * Reads randomly from the scratch file. One block is transferred. There is one scratch file connected to each terminal. It is opened for random read and write access when you log in. Its file number is 100B.
 *
 * Parameters:
 *   [I] BlockNumber (INTEGER): input
 *   [O] DataDestination (ARRAY): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_5B_ReadScratchFile(MonContext* ctx) {
    /* TODO: Implement ReadScratchFile (RDISK) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "BlockNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
