/*
 * MON 410B (264 decimal): FixInMemory (FIXMEM)
 *
 * Fixes a logical segment (either whole or in part) of a user's domain in physical memory. This action is intended to speed up access to the segment and is also useful for segment sharing.
 *
 * Parameters:
 *   [I] FixType (INTEGER): input
 *   [I] FirstAddr (INTEGER): input
 *   [I] Length (INTEGER): input
 *   [IO] ND100Addr (INTEGER): in/out
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_410B_FixInMemory(MonContext* ctx) {
    /* TODO: Implement FixInMemory (FIXMEM) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FixType");
    MON_LOG_IN_WORD(ctx, 1, "FirstAddr");
    MON_LOG_IN_WORD(ctx, 2, "Length");
    MON_LOG_IN_WORD(ctx, 3, "ND100Addr");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
