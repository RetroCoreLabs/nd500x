/*
 * MON 411B (265 decimal): MemoryUnfix (UNFIXM)
 *
 * Releases a fixed segment in your domain from physical memory. A fixed segment has all its pages fixed in physical memory. After MemoryUnfix the pages may be swapped between the disk and physical memory.
 *
 * Parameters:
 *   [I] Address (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_411B_MemoryUnfix(MonContext* ctx) {
    /* TODO: Implement MemoryUnfix (UNFIXM) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Address");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
