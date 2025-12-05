/*
 * MON 56B (46 decimal): SetUserParam (PASET)
 *
 * Sets information about a background program. Use GetUserParam to read the 5 parameters when a program is terminated.
 *
 * Parameters:
 *   [I] The five user parameters. (ARRAY): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_56B_SetUserParam(MonContext* ctx) {
    /* TODO: Implement SetUserParam (PASET) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "The five user parameters.");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
