/*
 * MON 57B (47 decimal): GetUserParam (PAGEI)
 *
 * Gets information about why the last program terminated. There are 5 parameters for each background user. These can be set by SINTRAN III or your background program.
 * 
 * - Use SetUserParam to set the parameter values.
 * - SINTRAN III sets some of the parameter values if you give the command @ENABLE-TERMINATION-HANDLING first.
 *
 * Parameters:
 *   [O] Buff (INTEGER2[5]): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_57B_GetUserParam(MonContext* ctx) {
    /* TODO: Implement GetUserParam (PAGEI) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
