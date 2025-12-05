/*
 * MON 307B (199 decimal): TerminalNoWait (TNOWAI)
 *
 * Switches No Wait on and off. No Wait is useful for input from, and output to, character devices, e.g. terminals. In No Wait, the program does not wait for input or output. Monitor calls like InByte return the error code 3 instead.
 * 
 * - SuspendProgram or WaitForRestart may passivate the program afterwards. The program then restarts when the device detects a break.
 * - The input buffer must be emptied before passivating the program.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [I] IOFlag (INTEGER2): input
 *   [I] NoWaitFlag (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_307B_TerminalNoWait(MonContext* ctx) {
    /* TODO: Implement TerminalNoWait (TNOWAI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "IOFlag");
    MON_LOG_IN_WORD(ctx, 2, "NoWaitFlag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
