/*
 * MON 55B (45 decimal): GetSpoolingEntry (RSQPE)
 *
 * Gets the next spooling queue entry, that is, the next file to be printed. The entry is removed from the spooling queue.
 *
 * Parameters:
 *   [I] SpoolDevNumber (INTEGER): input
 *   [O] Buffer (ARRAY): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_55B_GetSpoolingEntry(MonContext* ctx) {
    /* TODO: Implement GetSpoolingEntry (RSQPE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "SpoolDevNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
