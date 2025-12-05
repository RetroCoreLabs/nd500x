/*
 * MON 43B (35 decimal): CloseFile (CLOSE)
 *
 * Closes one or more files. Files must be opened before they are accessed. Afterwards they should be closed.
 * 
 * - **CloseFile** also resets peripheral files. This is similar to DeviceControl with control flag -1.
 * - For non-RT programs, files are closed when your program terminates.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_43B_CloseFile(MonContext* ctx) {
    /* TODO: Implement CloseFile (CLOSE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
