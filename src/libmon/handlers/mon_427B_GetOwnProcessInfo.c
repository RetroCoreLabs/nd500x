/*
 * MON 427B (279 decimal): GetOwnProcessInfo (GPRNME)
 *
 * Gets the name and number of your own process in the ND-500. You get a process each time you enter the ND-500 Monitor.
 * 
 * - The default process names are TERMINAL-xx where xx is a number.
 *
 * Parameters:
 *   [O] ProcessName (STRING): output
 *   [O] ProcessNumber (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_427B_GetOwnProcessInfo(MonContext* ctx) {
    /* TODO: Implement GetOwnProcessInfo (GPRNME) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
