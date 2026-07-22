/*
 * MON 44B (36 decimal): GetUserEntry (RUSER)
 *
 * Gets information about a user. The user entry in the directory is returned. It contains the user name, default file accesses, the pages in use, the password, the table of friends, and more.
 * 
 * - Only user RT and user SYSTEM may read the user entries of other users.
 *
 * Parameters:
 *   [I] UserName (STRING): input
 *   [O] Buff (ARRAY): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_44B_GetUserEntry(MonContext* ctx) {
    /* TODO: Implement GetUserEntry (RUSER) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "UserName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
