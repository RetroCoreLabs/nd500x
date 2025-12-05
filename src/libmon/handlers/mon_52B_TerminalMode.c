/*
 * MON 52B (42 decimal): TerminalMode (TERMO)
 *
 * Selects various terminal functions. You may stop output on full page. Input may be converted to uppercase letters. A delay after carriage return can be set. You may also set automatic logout if the line between the terminal and the computer is broken.
 * 
 * - Terminal mode can also be set on TADs.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [I] Mode (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_52B_TerminalMode(MonContext* ctx) {
    /* TODO: Implement TerminalMode (TERMO) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "Mode");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
