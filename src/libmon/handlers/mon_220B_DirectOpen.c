/*
 * MON 220B (144 decimal): DirectOpen (DOPEN)
 *
 * Opens a file. Files must be opened before they can be accessed. For public users this monitor call is identical to OpenFile. User SYSTEM and user RT are given the same access rights as the owner of a file.
 *
 * Parameters:
 *   [O] FileNumber (INTEGER2): output
 *   [I] AccessCode (INTEGER2): input
 *   [I] FileName (STRING): input
 *   [I] FileType (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_220B_DirectOpen(MonContext* ctx) {
    /* TODO: Implement DirectOpen (DOPEN) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 1, "AccessCode");
    MON_LOG_IN_WORD(ctx, 2, "FileName");
    MON_LOG_IN_WORD(ctx, 3, "FileType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
