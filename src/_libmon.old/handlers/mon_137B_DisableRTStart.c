/*
 * MON 137B (95 decimal): DisableRTStart (RTOFF)
 *
 * Disables start of RT programs. No RT program can be started before EnableRTStart is executed.
 * 
 * - RT programs in the time queue will not start. Other active RT programs are not affected.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_137B_DisableRTStart(MonContext* ctx) {
    /* TODO: Implement DisableRTStart (RTOFF) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
