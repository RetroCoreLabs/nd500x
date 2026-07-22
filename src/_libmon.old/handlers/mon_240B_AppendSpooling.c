/*
 * MON 240B (160 decimal): AppendSpooling (APSPE)
 *
 * Prints a file. The printer has a queue of files waiting to be output. The file is appended to this queue. One or more copies can be printed.
 * 
 * - You may connect a text, e.g. INVOICE, to the job. The operator uses the text to select all files to be printed on special paper.
 * - SINTRAN III, version K, allows both the file and the printer to be on remote systems. The complete standard syntax for remote file specification is as follows:
 * `system(user(password:project)).(directory:user)file:type;version`
 *
 * Parameters:
 *   [I] FileName (STRING): input
 *   [I] PrinterName (STRING): input
 *   [I] NoOfCopies (INTEGER): input
 *   [I] UserText (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_240B_AppendSpooling(MonContext* ctx) {
    /* TODO: Implement AppendSpooling (APSPE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileName");
    MON_LOG_IN_WORD(ctx, 1, "PrinterName");
    MON_LOG_IN_WORD(ctx, 2, "NoOfCopies");
    MON_LOG_IN_WORD(ctx, 3, "UserText");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
