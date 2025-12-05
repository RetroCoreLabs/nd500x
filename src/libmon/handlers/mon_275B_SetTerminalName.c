/*
 * MON 275B (189 decimal): SetTerminalName (STRFI)
 *
 * Defines the file name to be used for terminals. This is normally `TERMINAL:`. Background users identify their own terminal with this file name.
 * 
 * - You may use this monitor call more than once. Each file name will identify the terminals.
 *
 * Parameters:
 *   [I] TerminalName (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_275B_SetTerminalName(MonContext* ctx) {
    /* TODO: Implement SetTerminalName (STRFI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "TerminalName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
