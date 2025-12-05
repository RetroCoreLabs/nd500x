/*
 * MON 17B (15 decimal): SetTerminalType (MSTTY)
 *
 * Sets the type of a terminal. The terminal type tells SINTRAN III how to handle a particular terminal. A wrong terminal type normally distorts the screen. The function keys cannot be used.
 * 
 * - Appendix H lists the terminal types.
 * - Public background users may only set the terminal type for their own terminal. A background program must be run from user SYSTEM or RT to set the terminal type for another terminal.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [I] TerminalType (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_17B_SetTerminalType(MonContext* ctx) {
    /* TODO: Implement SetTerminalType (MSTTY) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "TerminalType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
