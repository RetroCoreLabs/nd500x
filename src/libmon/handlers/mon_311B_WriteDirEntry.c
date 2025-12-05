/*
 * MON 311B (201 decimal): WriteDirEntry (WDIEN)
 *
 * Changes the information about a directory. The complete contents of the directory entry is set. The SINTRAN III System Supervisor (ND-830003) describes the file system in more detail.
 * 
 * - The directory must be entered.
 * - The directory must be reserved.
 *
 * Parameters:
 *   [I] DirIndex (INTEGER): input
 *   [I] DirEntry (ARRAY): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_311B_WriteDirEntry(MonContext* ctx) {
    /* TODO: Implement WriteDirEntry (WDIEN) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DirIndex");
    MON_LOG_IN_WORD(ctx, 1, "DirEntry");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
