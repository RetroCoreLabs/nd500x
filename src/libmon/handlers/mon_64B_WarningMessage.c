/*
 * MON 64B (52 decimal): WarningMessage (ERMSG)
 *
 * Outputs a file system error message. The error message is output to the
 * terminal. In batch jobs, mode jobs, and RT programs it is output to the
 * error device (normally the console).
 *
 * - Error code 0 is illegal.
 * - The program continues after the message is output.
 *
 * Parameters (all are 32-bit WORD on ND-500):
 *   [I] ErrCode (W INTEGER): Error code number. See SINTRAN III appendix A.
 *
 * Note: On ND-500, INTEGER = 32-bit Word (W type).
 *
 * Reference: ND-860228.2 EN (SINTRAN III Monitor Calls)
 */

#include "../mon.h"
#include <stdio.h>

MonResult mon_64B_WarningMessage(MonContext* ctx) {
    int32_t error_code;

    /* Read the error code parameter as 32-bit word (ND-500 INTEGER = W) */
    error_code = (int32_t)mon_read_param_word(ctx, 0);

    /* Error code 0 is illegal per documentation */
    if (error_code == 0) {
        mon_log(MON_LOG_WARN, "MON 64B ERMSG: Error code 0 is illegal");
        mon_set_error(ctx, -1);
        return MON_ERROR;
    }

    /* Output the error message to console
     * TODO: Add actual SINTRAN III error message lookup table later
     */
    fprintf(stderr, "[SINTRAN ERROR %d]\n", error_code);

    /* Log the call */
    mon_log(MON_LOG_INFO, "MON 64B ERMSG: Error code %d", error_code);

    /* Set success - program continues */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
