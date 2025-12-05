/*
 * MON 142B (98 decimal): ToErrorDevice (ERMON)
 *
 * Outputs a user-defined, real-time error. The error message is output on the error device, i.e. normally the console. The following is an example of such a message: 23.10.59 ERROR 59 AT XPROG AT 134562, USER ERROR, SUBERROR 4. See appendix A.
 *
 * Parameters:
 *   [I] ErrorNumber (INTEGER): input
 *   [I] SubErrorNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_142B_ToErrorDevice(MonContext* ctx) {
    /* TODO: Implement ToErrorDevice (ERMON) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "ErrorNumber");
    MON_LOG_IN_WORD(ctx, 1, "SubErrorNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
