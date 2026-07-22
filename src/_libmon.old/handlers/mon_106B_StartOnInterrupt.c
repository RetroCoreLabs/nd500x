/*
 * MON 106B (70 decimal): StartOnInterrupt (CONCT)
 *
 * StartOnInterrupt connects an RT program to interrupts from a device. The RT program starts when an interrupt occurs. Use either WaitForRestart or SuspendProgram to make the program wait.
 * 
 * - You can remove this connection with NoInterruptStart.
 * - Several devices may be connected to one program.
 * - It is impossible to connect to some devices. Connection can be made if SINTRAN III has been generated with a connect driver routine for the device.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER2): input
 *   [I] DeviceNumber (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_106B_StartOnInterrupt(MonContext* ctx) {
    /* TODO: Implement StartOnInterrupt (CONCT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");
    MON_LOG_IN_WORD(ctx, 1, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
