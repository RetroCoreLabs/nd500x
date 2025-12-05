/*
 * MON 504B (324 decimal): OutputString (DVOUTS)
 *
 * Writes a string to a device, e.g. a terminal or an opened file.
 * 
 * - This is the most efficient way to output strings on the ND-500.
 * - The maximum string length is 2048 bytes.
 * - Appendix F contains an ASCII table.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER2): input
 *   [I] NoOfBytes (INTEGER2): input
 *   [I] Buffer (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_504B_OutputString(MonContext* ctx) {
    /* TODO: Implement OutputString (DVOUTS) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "NoOfBytes");
    MON_LOG_IN_WORD(ctx, 2, "Buffer");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
