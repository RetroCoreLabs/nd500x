/*
 * MON 67B (55 decimal): OutBufferSpace (OSIZE)
 *
 * Gets the number of free bytes in the output buffer (number of bytes which can be written before the program must wait). Terminals and other character devices place output in a buffer. Monitor calls like OutByte writes to this buffer.
 * 
 * - Use ExecutionInfo to get the logical device number for terminals. You can specify 1 for your own terminal.
 * - This monitor call is not available for internal devices. Use InBufferSpace and subtract this size from the inbuffer size.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [O] NoOfBytes (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_67B_OutBufferSpace(MonContext* ctx) {
    /* TODO: Implement OutBufferSpace (OSIZE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
