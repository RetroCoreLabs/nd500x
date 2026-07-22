/*
 * MON 21B (17 decimal): InUpTo8Bytes (M8INB)
 *
 * See also In8Bytes, InByte, InString, In4x2Bytes, and Out8Bytes.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [O] NoOfBytes (INTEGER2): output
 *   [O] InData (BYTES[8]): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_21B_InUpTo8Bytes(MonContext* ctx) {
    /* TODO: Implement InUpTo8Bytes (M8INB) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
