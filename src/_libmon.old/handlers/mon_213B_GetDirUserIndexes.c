/*
 * MON 213B (139 decimal): GetDirUserIndexes (MUIDI)
 *
 * Gets a directory index and a user index. You have to specify a directory name and a user name.
 * 
 * - Use ExecutionInfo to get the user index and the directory index of the user executing a program.
 *
 * Parameters:
 *   [I] UserName (STRING): input
 *   [O] DirIndex (INTEGER): output
 *   [O] UserIndex (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_213B_GetDirUserIndexes(MonContext* ctx) {
    /* TODO: Implement GetDirUserIndexes (MUIDI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "UserName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
