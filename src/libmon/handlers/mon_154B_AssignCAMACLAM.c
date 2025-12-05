/*
 * MON 154B (108 decimal): AssignCAMACLAM (ASSIG)
 *
 * Assigns a graded LAM in the CAMAC identification table to a logical device number in the logical number table. See CAMACFunction and CAMACGLRegister for more details.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [I] GradedLAMNumber (INTEGER): input
 *   [I] CrateNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_154B_AssignCAMACLAM(MonContext* ctx) {
    /* TODO: Implement AssignCAMACLAM (ASSIG) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "GradedLAMNumber");
    MON_LOG_IN_WORD(ctx, 2, "CrateNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
