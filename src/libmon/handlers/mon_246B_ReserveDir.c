/*
 * MON 246B (166 decimal): ReserveDir (REDIR)
 *
 * Reserves a directory for special use. The directory must be entered. Other users will not be able to open files on a reserved directory.
 * 
 * - All files in the directory must be closed.
 * - Only user RT and the current user may be logged in if main directory.
 * - Use ReleaseDir to release the directory.
 *
 * Parameters:
 *   [I] DirectoryIndex (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_246B_ReserveDir(MonContext* ctx) {
    /* TODO: Implement ReserveDir (REDIR) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DirectoryIndex");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
