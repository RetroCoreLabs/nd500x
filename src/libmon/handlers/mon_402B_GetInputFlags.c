/*
 * MON 402B (258 decimal): GetInputFlags (RFLAG)
 *
 * ND-100 and ND-500 programs may communicate through two 32-bit flag arrays. You can use the flags as you wish. GetInputFlags reads the input flags. The ND-100 sets these flags with the monitor call ND500Function. See the manual ND Linker User Guide and Reference Manual (ND-860289).
 * 
 * - You get the last values written to the flags. There is no queue.
 *
 * Parameters:
 *   [O] Value (INTEGER4): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_402B_GetInputFlags(MonContext* ctx) {
    /* TODO: Implement GetInputFlags (RFLAG) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
