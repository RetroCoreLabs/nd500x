/*
 * MON 206B (134 decimal): TerminationHandling (EDTRM)
 *
 * Switches termination handling on and off.
 *
 * Parameters:
 *   [I] EnDisFlag (INTEGER): input
 *   [I] Flag (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_206B_TerminationHandling(MonContext* ctx) {
    /* TODO: Implement TerminationHandling (EDTRM) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "EnDisFlag");
    MON_LOG_IN_WORD(ctx, 1, "Flag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
