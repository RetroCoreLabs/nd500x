/*
 * MON 334B (220 decimal): GetErrorMessage (GETXM)
 *
 * Gets a SINTRAN III error message text. Appendix A shows the messages connected to each error number. The error number is input. The program continues.
 * 
 * - This monitor call is convenient for advanced use of the terminal screen. For example, you may output the error message in inverse video at the bottom line.
 * - Do not input error number 0.
 *
 * Parameters:
 *   [I] ErrorNo (INTEGER): input
 *   [O] Buffer (STRING): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_334B_GetErrorMessage(MonContext* ctx) {
    /* TODO: Implement GetErrorMessage (GETXM) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "ErrorNo");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
