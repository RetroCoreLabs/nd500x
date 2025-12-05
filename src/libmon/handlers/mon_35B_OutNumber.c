/*
 * MON 35B (29 decimal): OutNumber (IOUT)
 *
 * Writes a number to the user's terminal. The number can be output as an octal or a decimal value.
 * 
 * - The number may be in the range -32768 to 32767.
 *
 * Parameters:
 *   [I] Format (INTEGER2): input
 *   [I] Number (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_35B_OutNumber(MonContext* ctx) {
    /* TODO: Implement OutNumber (IOUT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Format");
    MON_LOG_IN_WORD(ctx, 1, "Number");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
