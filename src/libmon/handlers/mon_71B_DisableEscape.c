/*
 * MON 71B (57 decimal): DisableEscape (DESCF)
 *
 * The ESCAPE key on the terminal normally terminates a program. This is called user break. This monitor call disables the escape function.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_71B_DisableEscape(MonContext* ctx) {
    /* TODO: Implement DisableEscape (DESCF) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
