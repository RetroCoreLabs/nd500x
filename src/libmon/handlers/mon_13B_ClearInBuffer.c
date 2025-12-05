/*
 * MON 13B (11 decimal): ClearInBuffer (CIBUF)
 *
 * Clears a device input buffer. Input from character devices, e.g. terminals, are temporarily stored in this buffer.
 * 
 * - You can use logical device number 1 for your own terminal in background programs.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_13B_ClearInBuffer(MonContext* ctx) {
    /* TODO: Implement ClearInBuffer (CIBUF) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
