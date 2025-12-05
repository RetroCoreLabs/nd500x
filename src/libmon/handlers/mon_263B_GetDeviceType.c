/*
 * MON 263B (179 decimal): GetDeviceType (GDEVT)
 *
 * Gets the device type, e.g. terminal, floppy disk, mass-storage file, etc. The monitor call also provides information on how to handle the device.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): input
 *   [I] IOFlag (INTEGER): input
 *   [O] DevType (INTEGER): output
 *   [O] DevAttr (INTEGER4): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_263B_GetDeviceType(MonContext* ctx) {
    /* TODO: Implement GetDeviceType (GDEVT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "IOFlag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
