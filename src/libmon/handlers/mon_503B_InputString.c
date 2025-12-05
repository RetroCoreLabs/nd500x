/*
 * MON 503B (323 decimal): InputString (DVINST)
 *
 * Reads a string from a device, e.g. a terminal or an opened file. This monitor call provide a fast input to ND-500 programs.
 *
 * Parameters:
 *   [I] DevNo (INTEGER): input
 *   [I] MaxNo (INTEGER): input
 *   [O] NoOfBytesRet (INTEGER): output
 *   [O] Buff (STRING): output
 *   [I] BreakStrat (INTEGER): input
 *   [I] EchoStrat (INTEGER): input
 *   [I] BreakT1 (INTEGER): input
 *   [I] BreakT2 (INTEGER): input
 *   [I] BreakT3 (INTEGER): input
 *   [I] BreakT4 (INTEGER): input
 *   [I] EchoT1 (INTEGER): input
 *   [I] EchoT2 (INTEGER): input
 *   [I] EchoT3 (INTEGER): input
 *   [I] EchoT4 (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_503B_InputString(MonContext* ctx) {
    /* TODO: Implement InputString (DVINST) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DevNo");
    MON_LOG_IN_WORD(ctx, 1, "MaxNo");
    MON_LOG_IN_WORD(ctx, 4, "BreakStrat");
    MON_LOG_IN_WORD(ctx, 5, "EchoStrat");
    MON_LOG_IN_WORD(ctx, 6, "BreakT1");
    MON_LOG_IN_WORD(ctx, 7, "BreakT2");
    MON_LOG_IN_WORD(ctx, 8, "BreakT3");
    MON_LOG_IN_WORD(ctx, 9, "BreakT4");
    MON_LOG_IN_WORD(ctx, 10, "EchoT1");
    MON_LOG_IN_WORD(ctx, 11, "EchoT2");
    MON_LOG_IN_WORD(ctx, 12, "EchoT3");
    MON_LOG_IN_WORD(ctx, 13, "EchoT4");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
