/*
 * MON 420B (272 decimal): GetUserRegisters (GRBLK)
 *
 * SwitchUserBreak allows you to save the registers when you terminate an ND-500 program with the ESCAPE key. You can get the contents of the registers with GetUserRegisters.
 *
 * Parameters:
 *   [I/O] Buffer (ARRAY): I/O
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_420B_GetUserRegisters(MonContext* ctx) {
    /* TODO: Implement GetUserRegisters (GRBLK) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
