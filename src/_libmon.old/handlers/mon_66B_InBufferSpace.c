/*
 * MON 66B (54 decimal): InBufferSpace (ISIZE)
 *
 * Gets the current number of bytes in the input buffer. Terminals and other character devices place input in a buffer. All input monitor calls read from this buffer.
 * 
 * - Use ExecutionInfo to get the logical device number for terminals. You can specify 1 for your own terminal.
 * - The buffer size depends on the device.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [O] NoOfBytes (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_66B_InBufferSpace(MonContext* ctx) {
    /* TODO: Implement InBufferSpace (ISIZE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
