/*
 * MON 63B (51 decimal): In4x2Bytes (B41NW)
 *
 * Reads 8 bytes from a word-oriented or character-oriented device, e.g. internal devices.
 * 
 * - Do not use this monitor call for terminals.
 * - This monitor call was mainly used for SIBAS communication via ND-NET. It is now seldom used.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [O] DataRead (BYTES[8]): output
 *   [O] NoOfBytes (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_63B_In4x2Bytes(MonContext* ctx) {
    /* TODO: Implement In4x2Bytes (B41NW) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
