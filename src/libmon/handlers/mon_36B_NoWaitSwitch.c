/*
 * MON 36B (30 decimal): NoWaitSwitch (NOWT)
 *
 * Switches No Wait on and off. No Wait is useful for input from, and output to several devices simultaneously. In No Wait, the program does not wait for input or output to complete. Monitor calls like InByte return error code 3 instead.
 * 
 * - **SuspendProgram** or **WaitForRestart** may passivate the program afterwards. The program restarts when input or output to the device is completed.
 * - For performance reasons, use **TerminalNoWait** (TNOWAI, mon 307) rather than NoWaitSwitch.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [I] IOFlag (INTEGER): input
 *   [I] WaitFlag (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_36B_NoWaitSwitch(MonContext* ctx) {
    /* TODO: Implement NoWaitSwitch (NOWT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "IOFlag");
    MON_LOG_IN_WORD(ctx, 2, "WaitFlag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
