/*
 * MON 24B (20 decimal): Out8Bytes (B8OUT)
 *
 * Writes 8 bytes to a character device, e.g. a terminal. All 8 bytes are output. OutUpTo8Bytes stops if a byte is 0.
 * 
 * - On the ND-500, you are advised to use the faster OutputString.
 * - Appendix F contains an ASCII table.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [I] OutData (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_24B_Out8Bytes(MonContext* ctx) {
    /* TODO: Implement Out8Bytes (B8OUT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "OutData");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
