/*
 * MON 232B (154 decimal): RenameFile (MRNFI)
 *
 * See also @RENAME-FILE.
 *
 * Parameters:
 *   [I] OldFileName (STRING): input
 *   [I] NewFileName (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_232B_RenameFile(MonContext* ctx) {
    /* TODO: Implement RenameFile (MRNFI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "OldFileName");
    MON_LOG_IN_WORD(ctx, 1, "NewFileName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
