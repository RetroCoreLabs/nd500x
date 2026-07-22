/*
 * MON 152B (106 decimal): GetRTName (GRTNA)
 *
 * Gets the name of an RT program. You specify the RT description address.
 * 
 * - This monitor call is only available to background programs in SINTRAN III VSX, version K.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER): input
 *   [O] RTProgramName (STRING): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_152B_GetRTName(MonContext* ctx) {
    /* TODO: Implement GetRTName (GRTNA) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
