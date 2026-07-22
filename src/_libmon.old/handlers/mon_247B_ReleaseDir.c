/*
 * MON 247B (167 decimal): ReleaseDir (RLDIR)
 *
 * Releases a directory. The directory must have been reserved with ReserveDir.
 *
 * Parameters:
 *   [I] DirectoryIndex (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_247B_ReleaseDir(MonContext* ctx) {
    /* TODO: Implement ReleaseDir (RLDIR) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DirectoryIndex");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
