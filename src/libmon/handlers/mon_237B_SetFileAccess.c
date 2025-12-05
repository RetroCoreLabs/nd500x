/*
 * MON 237B (159 decimal): SetFileAccess (SFACC)
 *
 * Sets the access protection for a file. You should specify the access for yourself, friends, and other users. The default file access for yourself is full access. Your friends have read access only. Other users have no access.
 * 
 * - You need directory access to a file to change the file access. User SYSTEM and RT may set the access protection for any files.
 * - Use the characters R, W, A, C, D, and N to specify the legal file access. R means Read. W means Write. A means Append to the end of a file. C means Common, i.e. more than one user may access the file at a time. D means Directory access, i.e. the file may be deleted, new versions created, etc. N means No access.
 * - Use @FILE-STATISTICS to check the file access.
 *
 * Parameters:
 *   [I] FileName (STRING): input
 *   [I] PublicAccess (STRING): input
 *   [I] FriendAccess (STRING): input
 *   [I] OwnAccess (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_237B_SetFileAccess(MonContext* ctx) {
    /* TODO: Implement SetFileAccess (SFACC) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileName");
    MON_LOG_IN_WORD(ctx, 1, "PublicAccess");
    MON_LOG_IN_WORD(ctx, 2, "FriendAccess");
    MON_LOG_IN_WORD(ctx, 3, "OwnAccess");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
