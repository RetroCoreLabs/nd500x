/*
 * MON 62B (50 decimal): GetBytesInFile (RMAX)
 *
 * Gets the number of bytes in a file. Only the bytes containing data are counted.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): input
 *   [O] NoOfBytes (INTEGER4): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_62B_GetBytesInFile(MonContext* ctx) {
    /* TODO: Implement GetBytesInFile (RMAX) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
