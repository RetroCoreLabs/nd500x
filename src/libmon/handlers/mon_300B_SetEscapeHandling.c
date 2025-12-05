/*
 * MON 300B (192 decimal): SetEscapeHandling (EUSEL)
 *
 * Enables user-defined escape handling. When the ESCAPE key is pressed, execution continues at the specified address in your program.
 *
 * Parameters:
 *   [I] EscapeHandler (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_300B_SetEscapeHandling(MonContext* ctx) {
    /* TODO: Implement SetEscapeHandling (EUSEL) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "EscapeHandler");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
