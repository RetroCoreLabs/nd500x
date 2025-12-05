/*
 * MON 22B (18 decimal): OutUpTo8Bytes (M8OUT)
 *
 * Writes up to 8 characters to a device, e.g. a terminal or an internal device.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): input
 *   [I] OutData (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_22B_OutUpTo8Bytes(MonContext* ctx) {
    /* TODO: Implement OutUpTo8Bytes (M8OUT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "OutData");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
