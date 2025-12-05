/*
 * MON 221B (145 decimal): CreateFile (CRALF)
 *
 * Creates a file. The file may be indexed, contiguous, or allocated. Most files are indexed. The size of indexed files expands automatically when written to. Contiguous and allocated files have shorter access time.
 * 
 * - You need directory access to the user who owns the file.
 * - User SYSTEM and RT always have the owner's access rights.
 * - An indexed file not yet written to may be converted to a contiguous file. Use ExpandFile or @EXPAND-FILE.
 *
 * Parameters:
 *   [I] FileName (STRING): input
 *   [I] StartAddress (INTEGER2): input
 *   [I] NoOfPages (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_221B_CreateFile(MonContext* ctx) {
    /* TODO: Implement CreateFile (CRALF) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileName");
    MON_LOG_IN_WORD(ctx, 1, "StartAddress");
    MON_LOG_IN_WORD(ctx, 2, "NoOfPages");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
