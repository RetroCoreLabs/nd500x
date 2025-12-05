/*
 * MON 314B (204 decimal): DefaultRemoteSystem (SRUSI)
 *
 * Sets default values for COSMOS remote file access. You can specify the default remote system, the remote user, and the remote user's passwords. The specified values are used when you omit values in a remote file access.
 * 
 * - Empty parameters remove previous default values. Default values are then the local user's name and passwords.
 * - SetRemoteMode switches the remote search on and off.
 *
 * Parameters:
 *   [I] SystemName (STRING): input
 *   [I] UserName (STRING): input
 *   [I] Password (STRING): input
 *   [I] ProjPassword (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_314B_DefaultRemoteSystem(MonContext* ctx) {
    /* TODO: Implement DefaultRemoteSystem (SRUSI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "SystemName");
    MON_LOG_IN_WORD(ctx, 1, "UserName");
    MON_LOG_IN_WORD(ctx, 2, "Password");
    MON_LOG_IN_WORD(ctx, 3, "ProjPassword");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
