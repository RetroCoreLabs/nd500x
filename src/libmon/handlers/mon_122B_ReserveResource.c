/*
 * MON 122B (82 decimal): ReserveResource (RESRV)
 *
 * Reserves a device or file for your program only. You release it with ReleaseResource. Some devices, e.g. terminals, have both an input and output part. You can only reserve one part with each ReserveResource call.
 * 
 * - A normal termination of an RT program releases all resources.
 * - Release the device with ReleaseResource or ForceRelease.
 * - A background program does not release a resource when you press the ESCAPE key.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER2): input
 *   [I] IOFlag (INTEGER2): input
 *   [I] WaitFlag (INTEGER2): input
 *   [O] Status (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_122B_ReserveResource(MonContext* ctx) {
    /* TODO: Implement ReserveResource (RESRV) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "IOFlag");
    MON_LOG_IN_WORD(ctx, 2, "WaitFlag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
