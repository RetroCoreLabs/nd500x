/*
 * MON 324B (212 decimal): OctobusFunction (OCTIO)
 *
 * Performs various functions on an old Octobus (earlier than version 3).
 *
 * Parameters:
 *   [I] FunctionCode (INTEGER2): input
 *   [I] DeviceNo (INTEGER2): input
 *   [IO] Parameter (INTEGER2): in/out
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_324B_OctobusFunction(MonContext* ctx) {
    /* TODO: Implement OctobusFunction (OCTIO) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FunctionCode");
    MON_LOG_IN_WORD(ctx, 1, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 2, "Parameter");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
