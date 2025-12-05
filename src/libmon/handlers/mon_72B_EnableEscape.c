/*
 * MON 72B (58 decimal): EnableEscape (EESCF)
 *
 * Enables the ESCAPE key on the terminal. The ESCAPE key normally terminates a program. This is called user break. You can disable this key with DisableEscape. To enable it again you should use EnableEscape.
 * 
 * - The escape function is enabled when you log out.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_72B_EnableEscape(MonContext* ctx) {
    /* TODO: Implement EnableEscape (EESCF) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
