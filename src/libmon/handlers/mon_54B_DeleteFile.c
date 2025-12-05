/*
 * MON 54B (44 decimal): DeleteFile (MDLFI)
 *
 * Deletes a file. The pages of the file are released.
 * 
 * - You must have directory access to the file in order to delete it. RT programs can delete a file if user RT has directory access to it.
 * - Include a version number in the file name to delete specific versions of a file. Otherwise, all versions are deleted.
 *
 * Parameters:
 *   [I] FileName (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_54B_DeleteFile(MonContext* ctx) {
    /* TODO: Implement DeleteFile (MDLFI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
