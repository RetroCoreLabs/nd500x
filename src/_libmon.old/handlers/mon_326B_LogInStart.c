/*
 * MON 326B (214 decimal): LogInStart (MLOGI)
 *
 * Logs in a user on a terminal and starts a subsystem.
 *
 * Parameters:
 *   [I] TermNo (INTEGER): input
 *   [I] UserName (STRING): input
 *   [I] Password (STRING): input
 *   [I] ProjPassword (STRING): input
 *   [I] Subsystem (STRING): input
 *   [I] UserParam (ARRAY): input
 *   [O] Status (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_326B_LogInStart(MonContext* ctx) {
    /* TODO: Implement LogInStart (MLOGI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "TermNo");
    MON_LOG_IN_WORD(ctx, 1, "UserName");
    MON_LOG_IN_WORD(ctx, 2, "Password");
    MON_LOG_IN_WORD(ctx, 3, "ProjPassword");
    MON_LOG_IN_WORD(ctx, 4, "Subsystem");
    MON_LOG_IN_WORD(ctx, 5, "UserParam");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
