/*
 * MON 40B (32 decimal): CloseSpoolingFile (SPCLO)
 *
 * Appends an opened file to a spooling queue. You specify a text to be printed on the error device when the file is to be printed.
 * 
 * - If the file is not a spooling file, a normal close is performed.
 * - Does not work for remote files. For these, a normal close is performed.
 *
 * Parameters:
 *   [I] FileNo (INTEGER2): input
 *   [I] UserText (STRING): input
 *   [I] NoOfCopies (INTEGER2): input
 *   [I] PrintFlag (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_40B_CloseSpoolingFile(MonContext* ctx) {
    /* TODO: Implement CloseSpoolingFile (SPCLO) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNo");
    MON_LOG_IN_WORD(ctx, 1, "UserText");
    MON_LOG_IN_WORD(ctx, 2, "NoOfCopies");
    MON_LOG_IN_WORD(ctx, 3, "PrintFlag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
