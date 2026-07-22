/*
 * MON 140B (96 decimal): ReservationInfo (WHDEV)
 *
 * Checks that a device is not reserved. If it is reserved, you will receive information about which RT program that reserves it.
 * 
 * - Some devices have both an input and an output part. You have to use ReservationInfo for each part.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [I] IOFlag (INTEGER2): input
 *   [O] ReturnValue (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_140B_ReservationInfo(MonContext* ctx) {
    /* TODO: Implement ReservationInfo (WHDEV) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "IOFlag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
