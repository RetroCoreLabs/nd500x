/*
 * MON 107B (71 decimal): NoInterruptStart (DSCNT)
 *
 * StartOnInterrupt connects an RT program to interrupts from a device. You remove this connection with NoInterruptStart.
 * 
 * - The program may be in the time queue. It is then removed. Periodic execution is prevented.
 * - Reserved resources are not released.
 * - The program is not removed from the execution queue.
 *
 * Parameters:
 *   [I] RTProgram (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_107B_NoInterruptStart(MonContext* ctx) {
    /* TODO: Implement NoInterruptStart (DSCNT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RTProgram");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
