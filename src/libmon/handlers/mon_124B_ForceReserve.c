/*
 * MON 124B (84 decimal): ForceReserve (PRSRV)
 *
 * Reserves a device for an RT program other than that which is calling. Use ForceRelease if the device is already reserved.
 * 
 * - You can only release peripheral devices, such as terminals and printers, and semaphores in this way.
 * - Programs in a waiting queue may reserve the device between your ForceRelease and ForceReserve calls if they have higher priority than your program.
 * - The SINTRAN III Real Time Guide (ND-860133) describes this in more detail.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER2): input
 *   [I] IOFlag (INTEGER2): input
 *   [I] RTProgram (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_124B_ForceReserve(MonContext* ctx) {
    /* TODO: Implement ForceReserve (PRSRV) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "IOFlag");
    MON_LOG_IN_WORD(ctx, 2, "RTProgram");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
