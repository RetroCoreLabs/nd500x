/*
 * MON 256B (174 decimal): FullFileName (DEABF)
 *
 * Returns a complete file name from an abbreviated one. The directory, the user, the file name, the file type, and the version are returned.
 *
 * Parameters:
 *   [I] AbbrevFileName (STRING): input
 *   [O] FileName (STRING): output
 *   [I] FileType (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_256B_FullFileName(MonContext* ctx) {
    /* TODO: Implement FullFileName (DEABF) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "AbbrevFileName");
    MON_LOG_IN_WORD(ctx, 2, "FileType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
