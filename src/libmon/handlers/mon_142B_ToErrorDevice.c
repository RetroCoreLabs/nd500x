/*
 * MON 142B (98 decimal): ToErrorDevice (ERMON)
 *
 * Outputs a user-defined, real-time error. The error message is output on the
 * error device, i.e. normally the console.
 *
 * Example output format:
 *   "23.10.59 ERROR 59 AT XPROG AT 134562, USER ERROR, SUBERROR 4"
 *
 * Parameters (all are 32-bit WORD on ND-500):
 *   [I] ErrorNumber (W INTEGER): Error number (50-69). This number is output
 *       following "ERROR".
 *   [I] SubErrorNumber (W INTEGER): Suberror number.
 *
 * Note: On ND-500, INTEGER = 32-bit Word (W type).
 *
 * Reference: ND-860228.2 EN (SINTRAN III Monitor Calls)
 */

#include "../mon.h"
#include <stdio.h>
#include <time.h>

MonResult mon_142B_ToErrorDevice(MonContext* ctx) {
    int32_t error_number;
    int32_t suberror_number;
    time_t now;
    struct tm* tm_info;

    /* Read input parameters as 32-bit words (ND-500 INTEGER = W) */
    error_number = (int32_t)mon_read_param_word(ctx, 0);
    suberror_number = (int32_t)mon_read_param_word(ctx, 1);

    /* Get current time for timestamp */
    time(&now);
    tm_info = localtime(&now);

    /* Output error message to stderr (error device/console)
     * Format similar to SINTRAN: "HH.MM.SS ERROR nn, USER ERROR, SUBERROR m"
     * Numbers in octal format per SINTRAN convention
     */
    fprintf(stderr, "%02d.%02d.%02d ERROR %oB, USER ERROR, SUBERROR %oB\n",
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
            (unsigned int)error_number, (unsigned int)suberror_number);

    /* Log the call */
    mon_log(MON_LOG_INFO, "MON 142B ERMON: ErrorNumber=%oB, SubErrorNumber=%oB",
            (unsigned int)error_number, (unsigned int)suberror_number);

    /* Set success - program continues */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
