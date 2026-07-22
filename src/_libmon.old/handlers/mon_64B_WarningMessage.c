/*
 * MON 64B [ERMSG/WarningMessage]
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
#include "../mon_log.h"
#include <stdio.h>

MonResult mon_64B_WarningMessage(MonContext* ctx) {
    int32_t error_code;

    /* Read the error code parameter as 32-bit word (ND-500 INTEGER = W) */
    error_code = (int32_t)mon_read_param_word(ctx, 0);

    /* Per the carved L07 ERMSG worker (dual entry ERMSG/QERMS @16714B, closing
     * at 17020B EXIT): the body has NO error/K return path - it looks up the
     * message text, writes it, and always continues. "Error code 0 is illegal"
     * is a caller-side manual note, NOT a status this call returns. So do not
     * fabricate an error return for code 0; just log and continue. */
    if (error_code == 0) {
        mon_log(MON_LOG_WARN, MON_ID_64B ": Error code 0 (caller-side illegal; continuing)");
    }

    /* Output the error message to console in octal format per SINTRAN convention
     * TODO: Add actual SINTRAN III error message lookup table later
     */
    fprintf(stderr, "[SINTRAN ERROR %oB]\n", (unsigned int)error_code);

    /* Log the call */
    mon_log(MON_LOG_INFO, MON_ID_64B ": IN: ErrCode=%oB", (unsigned int)error_code);

    /* Set success - program continues */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
