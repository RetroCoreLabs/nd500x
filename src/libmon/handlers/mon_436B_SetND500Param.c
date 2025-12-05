/*
 * MON 436B (286 decimal): SetND500Param (5PASET)
 *
 * Sets information about an ND-500 program. Use GetND500Param to read the 5 parameters when a program is terminated.
 * 
 * - SINTRAN III sets some of the parameter values if you give the command @ENABLE-TERMINATION-HANDLING first.
 *
 * Parameters:
 *   [I] Buffer (INTEGER2[5]): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_436B_SetND500Param(MonContext* ctx) {
    /* TODO: Implement SetND500Param (5PASET) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Buffer");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
