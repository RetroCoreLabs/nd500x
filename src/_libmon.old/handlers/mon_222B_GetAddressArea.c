/*
 * MON 222B (146 decimal): GetAddressArea (GBSIZ)
 *
 * Gets the size of your address area. Your address area may consist of one or two 128 Kbyte areas. This depends on the size of the background segments.
 *
 * Parameters:
 *   [O] SegmentSize (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_222B_GetAddressArea(MonContext* ctx) {
    /* TODO: Implement GetAddressArea (GBSIZ) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
