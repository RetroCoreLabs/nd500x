/*
 * MON 505B (325 decimal): GetTrapReason (GERRCOD)
 *
 * Gets the error code from the swapper process. This is only relevant to programmed trap handlers. The swapper process starts the trap handler when it detects a fatal error, e.g. address outside segment. Use this monitor call to get the error code.
 *
 * Parameters:
 *   [O] ErrorCode (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_505B_GetTrapReason(MonContext* ctx) {
    /* TODO: Implement GetTrapReason (GERRCOD) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
