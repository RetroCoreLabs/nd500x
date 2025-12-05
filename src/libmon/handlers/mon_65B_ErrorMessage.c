/*
 * MON 65B (53 decimal): ErrorMessage (QERMS)
 *
 * Displays a file system error message. Appendix A shows the messages connected to each error number. The error number is input. The program terminates.
 * 
 * - The error message is displayed on the terminal. RT programs write it to the error device. The error device is normally the console.
 * - Do not input error number 0.
 *
 * Parameters:
 *   [I] ErrNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_65B_ErrorMessage(MonContext* ctx) {
    /* TODO: Implement ErrorMessage (QERMS) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "ErrNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
