/*
 * MON 413B (267 decimal): FileNotAsSegment (FSCDNT)
 *
 * Disconnects a file as a segment in your domain. FileAsSegment allows files to be accessed as segments. This monitor call disconnects the file.
 * 
 * - The file is not closed.
 * - The file is automatically disconnected by CloseFile.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER2): input
 *   [I] LogSegmentNumber (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_413B_FileNotAsSegment(MonContext* ctx) {
    /* TODO: Implement FileNotAsSegment (FSCDNT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "LogSegmentNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
