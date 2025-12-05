/*
 * MON 270B (184 decimal): ReadDiskPage (RDPAG)
 *
 * Reads one or more directory pages. Any page can be read.
 * 
 * - The directory must be reserved with ReserveDir.
 *
 * Parameters:
 *   [I] DirIndex (INTEGER): input
 *   [O] Buffer (ARRAY): output
 *   [I] PageAddr (INTEGER): input
 *   [I] NoOfPages (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_270B_ReadDiskPage(MonContext* ctx) {
    /* TODO: Implement ReadDiskPage (RDPAG) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DirIndex");
    MON_LOG_IN_WORD(ctx, 2, "PageAddr");
    MON_LOG_IN_WORD(ctx, 3, "NoOfPages");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
