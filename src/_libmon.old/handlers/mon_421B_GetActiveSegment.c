/*
 * MON 421B (273 decimal): GetActiveSegment (GASGM)
 *
 * Gets the name of the segments in your domain. A 2048 byte buffer is returned. It contains 32 pointers to segment names in the buffer. Each pointer consists of 12 bytes. The first four is an address. The second four is the offset from this address to the start of the segment name. The last four are the offset to the end of the segment name.
 *
 * Parameters:
 *   [O] Buffer (INTEGER2[1024]): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_421B_GetActiveSegment(MonContext* ctx) {
    /* TODO: Implement GetActiveSegment (GASGM) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
