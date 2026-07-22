/*
 * MON 27B (23 decimal): GetRTDescr (RTDSC)
 *
 * Reads an RT description. The RT description contains various information about an RT program. You specify the RT program address. See the SINTRAN III Real Time Guide (ND-860133) for further details.
 * 
 * - Use GetRTAddress if you only know the name of the RT program.
 * - This monitor call is only available to background programs in SINTRAN III VSX, version K.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER): input
 *   [O] RTDescriptor (ARRAY): output
 *   [O] NoOfConnDev (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_27B_GetRTDescr(MonContext* ctx) {
    /* TODO: Implement GetRTDescr (RTDSC) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
