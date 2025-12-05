/*
 * MON 16B (14 decimal): GetTerminalType (MGTTY)
 *
 * Gets the terminal type. The terminal type tells SINTRAN III how to handle a particular terminal. A wrong terminal type normally distorts the screen. The function-keys cannot be used.
 * 
 * - Appendix H lists the terminal types.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [O] TerminalType (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_16B_GetTerminalType(MonContext* ctx) {
    /* TODO: Implement GetTerminalType (MGTTY) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
