/*
 * MON 207B (135 decimal): GetErrorInfo (RERRP)
 *
 * Gets information about the last real-time error. The monitor call returns the error, the RT program responsible for the error, and the program address where it occurred. A flag tells whether the RT program was aborted or not.
 *
 * Parameters:
 *   [O] Buffer (ARRAY): output
 *   [O] ReturnStatus (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_207B_GetErrorInfo(MonContext* ctx) {
    /* TODO: Implement GetErrorInfo (RERRP) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
