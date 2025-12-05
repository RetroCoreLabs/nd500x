/*
 * MON 11B (9 decimal): GetBasicTime (TIME)
 *
 * **Time**
 * 
 * Gets the current internal time. The internal time is specified in basic time units. There are 50 basic time units in a second.
 * 
 * - The internal time is set to 0 each time SINTRAN III is started.
 *
 * Parameters:
 *   [O] BasicTime (LONGINT): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_11B_GetBasicTime(MonContext* ctx) {
    /* TODO: Implement GetBasicTime (TIME) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
