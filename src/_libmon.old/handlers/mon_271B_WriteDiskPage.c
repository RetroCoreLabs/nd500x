/*
 * MON 271B (185 decimal): WriteDiskPage (WDPAG)
 *
 * Writes to one or more pages in a directory. Any page can be written to.
 * 
 * - The directory must be reserved with ReseveDir.
 *
 * Parameters:
 *   [I] DirIndex (INTEGER): input
 *   [I] Buffer (ARRAY): input
 *   [I] PageAddr (INTEGER): input
 *   [I] NoOfPages (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_271B_WriteDiskPage(MonContext* ctx) {
    /* TODO: Implement WriteDiskPage (WDPAG) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DirIndex");
    MON_LOG_IN_WORD(ctx, 1, "Buffer");
    MON_LOG_IN_WORD(ctx, 2, "PageAddr");
    MON_LOG_IN_WORD(ctx, 3, "NoOfPages");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
