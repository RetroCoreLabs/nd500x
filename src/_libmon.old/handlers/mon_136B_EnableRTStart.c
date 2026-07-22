/*
 * MON 136B (94 decimal): EnableRTStart (RTON)
 *
 * RTON RT programs cannot be started after DisableRTStart has been executed. Use EnableRTStart to do this.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_136B_EnableRTStart(MonContext* ctx) {
    /* TODO: Implement EnableRTStart (RTON) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
