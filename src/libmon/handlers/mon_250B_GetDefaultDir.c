/*
 * MON 250B (168 decimal): GetDefaultDir (FDFDI)
 *
 * Gets the user?s default directory. The directory index and the user index are returned.
 * 
 * - Use ExecutionInfo to get the user index and the directory index of the user executing the program.
 *
 * Parameters:
 *   [I] UserName (STRING): input
 *   [O] DirectoryIndex (INTEGER): output
 *   [O] UserIndex (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_250B_GetDefaultDir(MonContext* ctx) {
    /* TODO: Implement GetDefaultDir (FDFDI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "UserName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
