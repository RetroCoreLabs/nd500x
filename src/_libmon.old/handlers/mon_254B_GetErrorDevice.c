/*
 * MON 254B (172 decimal): GetErrorDevice (GERDV)
 *
 * Gets the logical device number of the error device. The error device may be reserved by an RT program. If this is the case, the monitor call returns the address of the RT description. The error device is the terminal which outputs system errors and RT program messages. The error device is normally the console.
 *
 * Parameters:
 *   [O] ErrorDevice (INTEGER): output
 *   [O] RTProgram (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_254B_GetErrorDevice(MonContext* ctx) {
    /* TODO: Implement GetErrorDevice (GERDV) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
