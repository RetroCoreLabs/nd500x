/*
 * MON 125B (85 decimal): ForceRelease (PRLRS)
 *
 * Releases a device reserved by an RT program other than that which is calling. You can then reserve the device for your own RT program. Some devices, such as terminals, have both an input and output part. You can only release one part with each ForceRelease.
 * 
 * - Use ReservationInfo to get the RT description address of the reserving RT program. You may then give the device back with ForceReserve.
 * - Programs in a waiting queue may reserve the device between your ForceRelease and ReserveResource calls if they have higher priority than your program.
 * - The SINTRAN III Real Time Guide (ND-860133) describes this in more detail.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [I] IOFlag (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_125B_ForceRelease(MonContext* ctx) {
    /* TODO: Implement ForceRelease (PRLRS) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "IOFlag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
