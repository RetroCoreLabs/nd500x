/*
 * MON 257B (175 decimal): OpenFileInfo (FOPEN)
 *
 * Gets information about an open file. You specify the file name. The monitor call returns the file number and the access type. The logical device number of peripheral equipment is returned for peripheral files. Accepts COSMOS RFA.
 *
 * Parameters:
 *   [I] FileName (STRING): input
 *   [I] FileType (STRING): input
 *   [O] FileNo (INTEGER2): output
 *   [O] AccessCode (INTEGER2): output
 *   [O] DevNo (INTEGER2): output
 *   [O] ErrCode (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_257B_OpenFileInfo(MonContext* ctx) {
    /* TODO: Implement OpenFileInfo (FOPEN) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileName");
    MON_LOG_IN_WORD(ctx, 1, "FileType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
