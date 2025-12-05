/*
 * MON 14B (12 decimal): ClearOutBuffer (COBUF)
 *
 * Clears a device output buffer. Output to character devices, e.g. terminals, are temporarily stored in this buffer.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_14B_ClearOutBuffer(MonContext* ctx) {
    /* TODO: Implement ClearOutBuffer (COBUF) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
