/*
 * MON 151B (105 decimal): GetRTAddress (GRTDA)
 *
 * Gets the address of an RT description. You specify the name of the RT program. See the SINTRAN III Real Time Guide (ND-860133) for further information.
 *
 * Parameters:
 *   [I] RTProgramName (STRING): input
 *   [O] RTProgram (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_151B_GetRTAddress(MonContext* ctx) {
    /* TODO: Implement GetRTAddress (GRTDA) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgramName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
