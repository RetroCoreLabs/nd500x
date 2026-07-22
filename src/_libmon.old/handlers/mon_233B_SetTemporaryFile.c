/*
 * MON 233B (155 decimal): SetTemporaryFile (STEFI)
 *
 * Defines a file to store information temporarily. The file can be read once. When it is closed, its contents are deleted. The empty file will still exist.
 * 
 * - GetObjectEntry and @FILE-STATISTICS shows whether a file is temporary or not.
 * - Files to be printed are commonly defined as temporary. Their contents exist until they have been printed.
 *
 * Parameters:
 *   [I] FileName (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_233B_SetTemporaryFile(MonContext* ctx) {
    /* TODO: Implement SetTemporaryFile (STEFI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
