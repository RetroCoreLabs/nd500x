/*
 * MON 405B (261 decimal): SwitchUserBreak (USTRK)
 *
 * Switches user-defined escape handling on and off. The user-defined escape handling transfers control to a routine when you press the ESCAPE key.
 *
 * Parameters:
 *   [I] OnOffFlag (INTEGER2): input
 *   [I] Address (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_405B_SwitchUserBreak(MonContext* ctx) {
    /* TODO: Implement SwitchUserBreak (USTRK) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "OnOffFlag");
    MON_LOG_IN_WORD(ctx, 1, "Address");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
