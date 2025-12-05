/*
 * MON 214B (140 decimal): GetUserName (GUSNA)
 *
 * Gets the name of a user. The user may be on a remote computer if the COSMOS network is installed. The remote system name is then returned.
 * 
 * - RT programs return the name of user RT.
 *
 * Parameters:
 *   [O] UserName (STRING): output
 *   [I] DirectoryIndex (INTEGER): input
 *   [I] UserIndex (INTEGER): input
 *   [O] RemoteFlag (INTEGER): output
 *   [O] RemoteSystem (STRING): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_214B_GetUserName(MonContext* ctx) {
    /* TODO: Implement GetUserName (GUSNA) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 1, "DirectoryIndex");
    MON_LOG_IN_WORD(ctx, 2, "UserIndex");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
