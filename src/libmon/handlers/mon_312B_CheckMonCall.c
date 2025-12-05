/*
 * MON 312B (202 decimal): CheckMonCall (MOINF)
 *
 * Some monitor calls are optional or only available in later versions of SINTRAN III. This monitor call checks if a monitor call exists in your particular SINTRAN III system. Optional monitor calls are included or left out when SINTRAN III is generated.
 *
 * Parameters:
 *   [I] MonCallNumber (INTEGER): input
 *   [O] MonCallEntry (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_312B_CheckMonCall(MonContext* ctx) {
    /* TODO: Implement CheckMonCall (MOINF) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "MonCallNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
