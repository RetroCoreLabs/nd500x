/*
 * MON 123B (83 decimal): ReleaseResource (RELES)
 *
 * Releases a reserved device or file. The resource can then be used by another program. You reserve a device or opened file with ReserveResource. Some devices, e.g. terminals, have both an input and output part. You can only release one part with each ReleaseResource.
 * 
 * - A normal termination of an RT program release all resources.
 * - Reserve the device with ReserveResource or ForceReserve.
 * - CloseFile or @CLOSE-FILE releases reserved files.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [I] IOFlag (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_123B_ReleaseResource(MonContext* ctx) {
    /* TODO: Implement ReleaseResource (RELES) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "IOFlag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
