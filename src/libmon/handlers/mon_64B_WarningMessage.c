/*
 * MON 64B (52 decimal): WarningMessage (ERMSG)
 *
 * Outputs a file system error message. Appendix A shows the messages connected to each error code. The error code is input. The program continues.
 * 
 * - The error message is output to the terminal. In batch jobs, mode jobs, and RT programs it is output to the error device. The error device is normally the console.
 * - Error code 0 is illegal.
 *
 * Parameters:
 *   [I] ErrCode (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_64B_WarningMessage(MonContext* ctx) {
    /* TODO: Implement WarningMessage (ERMSG) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "ErrCode");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
