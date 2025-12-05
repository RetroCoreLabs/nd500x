/*
 * MON 33B (27 decimal): AltPageTable (ALTON)
 *
 * Switches page table. Each page table allows you to access 128 Kbyte memory. SINTRAN III has 4 page tables. SINTRAN III VSX, version K, has 16 page tables. They are numbered 0-15. RT programs may use any page table. Background programs will get page table 2. The parameter is ignored.
 *
 * Parameters:
 *   [I] PageTableNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_33B_AltPageTable(MonContext* ctx) {
    /* TODO: Implement AltPageTable (ALTON) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "PageTableNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
