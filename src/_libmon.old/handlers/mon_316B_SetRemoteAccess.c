/*
 * MON 316B (206 decimal): SetRemoteAccess (SRLMO)
 *
 * Switches remote file access on and off. The COSMOS network allows you to access files in remote computers directly. Use DefaultRemoteSystem or QSET-DEFAULT-REMOTE-SYSTEM to specify a default remote system. If a file does not exist in the local system, the default remote system is searched. SetRemoteAccess switches this function on and off.
 * 
 * - You may include a remote system identification in the file name. Only the specified system is searched.
 * - This monitor call is only available with COSMOS.
 *
 * Parameters:
 *   [I] Mode (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_316B_SetRemoteAccess(MonContext* ctx) {
    /* TODO: Implement SetRemoteAccess (SRLMO) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Mode");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
