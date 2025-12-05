/*
 * MON 30B (24 decimal): GetOwnRTAddress (GETRT)
 *
 * Gets the address of the calling program's RT description. Background programs get the RT description address of the RT program which controls the terminal.
 *
 * Parameters:
 *   [O] RTDescrAddress (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_30B_GetOwnRTAddress(MonContext* ctx) {
    /* TODO: Implement GetOwnRTAddress (GETRT) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
