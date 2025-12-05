/*
 * MON 231B (153 decimal): ExpandFile (EXPFI)
 *
 * Expands the file size. You use this monitor call to increase the size of contiguous and allocated files. The space following the file on the disk must be free.
 * 
 * - Indexed files created with 0 pages may be expanded.
 * - Public users must have directory access to the file. User RT and SYSTEM can expand any file.
 *
 * Parameters:
 *   [I] FileName (STRING): input
 *   [I] NoOfPages (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_231B_ExpandFile(MonContext* ctx) {
    /* TODO: Implement ExpandFile (EXPFI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileName");
    MON_LOG_IN_WORD(ctx, 1, "NoOfPages");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
