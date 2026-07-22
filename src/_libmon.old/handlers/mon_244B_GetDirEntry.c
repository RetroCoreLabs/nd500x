/*
 * MON 244B (164 decimal): GetDirEntry (GDIEN)
 *
 * Gets information about a directory. The directory entry is returned. Appendix C describes the file system in more detail.
 * 
 * - You may access directories on remote systems. The computers must be connected through a COSMOS network.
 *
 * Parameters:
 *   [I] DirectoryIndex (INTEGER2): input
 *   [O] DirEntry (INTEGER2[21]): output
 *   [O] Flag (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_244B_GetDirEntry(MonContext* ctx) {
    /* TODO: Implement GetDirEntry (GDIEN) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DirectoryIndex");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
