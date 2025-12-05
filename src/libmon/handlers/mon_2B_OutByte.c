/*
 * MON 2B (2 decimal): OutByte (OUTBT)
 *
 * Writes one byte to a character device, e.g. a terminal or an opened file. If the device is a word-oriented device, one word is written.
 * 
 * - The program waits if the output buffer of the device is full. You can change this with NoWaitSwitch or TerminalNoWait.
 * - The pointer to the next byte is incremented when you write to a mass-storage file.
 * - Output from card readers are converted to ASCII characters. Use DeviceControl to write the 12-bit card columns.
 * - You are advised to use the faster OutputString on the ND-500.
 * - Appendix F contains an ASCII table.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [I] OutputValue (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_2B_OutByte(MonContext* ctx) {
    /* TODO: Implement OutByte (OUTBT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "OutputValue");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
