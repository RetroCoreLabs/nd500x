/*
 * MON 50B (40 decimal): OpenFile (OPEN)
 *
 * Opens a file. You cannot access a file before you open it. Specify what kind of access you want, e.g. sequential write or random read.
 *
 * Parameters:
 *   [IO] FileNo (INTEGER): in/out
 *   [I] AccessCode (INTEGER): input
 *   [I] FileName (STRING): input
 *   [I] FileType (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_50B_OpenFile(MonContext* ctx) {
    /* TODO: Implement OpenFile (OPEN) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNo");
    MON_LOG_IN_WORD(ctx, 1, "AccessCode");
    MON_LOG_IN_WORD(ctx, 2, "FileName");
    MON_LOG_IN_WORD(ctx, 3, "FileType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
