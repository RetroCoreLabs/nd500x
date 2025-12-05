/*
 * MON 23B (19 decimal): In8Bytes (B8INB)
 *
 * Reads 8 bytes from a device. The input is fast, but the monitor call does not apply the defined echo and break setting.
 * 
 * - Do not use this monitor call for terminals and TAD's when echo and break should be applied.
 * - Appendix F contains an ASCII table.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [O] NoOfBytes (INTEGER): output
 *   [O] DataRead (STRING): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_23B_In8Bytes(MonContext* ctx) {
    /* TODO: Implement In8Bytes (B8INB) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
