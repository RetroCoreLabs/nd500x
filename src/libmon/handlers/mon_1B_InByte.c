/*
 * MON 1B (1 decimal): InByte (INBT)
 *
 * Reads one byte from a character device, e.g. a terminal or an opened file. If the device is a word-oriented device, one word is read. This monitor call can be used on most input devices.
 * 
 * - Bit 7 is a parity bit if terminal or file input. IOMultiFunction may change this.
 * - The program waits if there is no bytes in the input buffer of the device. You can change this with NoWaitSwitch or TerminalNoWait.
 * - The pointer to the next byte is incremented when you read from a mass-storage file.
 * - Input from card readers are converted to ASCII characters. Use DeviceControl to read the 12-bit card columns.
 * - Background programs may read from logical device number 0. This is the SINTRAN III command buffer. You may read parameters following the program name this way. Break and echo are both set to 1. Normal SINTRAN III command editing is available. All letters are converted to uppercase. You may control this with IOMultiFunction.
 * - Appendix F contains an ASCII table.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [O] ReturnValue (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_1B_InByte(MonContext* ctx) {
    /* TODO: Implement InByte (INBT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
