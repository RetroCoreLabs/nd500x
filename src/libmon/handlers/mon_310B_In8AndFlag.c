/*
 * MON 310B (200 decimal): In8AndFlag (TBIN8)
 *
 * Reads 8 bytes from a device, e.g., a terminal. The monitor call applies to the defined echo and break setting. See SetEcho and SetBreak.
 * 
 * - Input of a break character stops the reading. The number of characters read are output. It is output as a negative number if a break character has been read. That is, bit 15 is set to 1.
 * - This monitor call can be used together with SetBreak and SetEcho.
 * - Appendix F contains an ASCII table.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [O] NoOfBytes (INTEGER): output
 *   [O] Buffer (STRING): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_310B_In8AndFlag(MonContext* ctx) {
    /* TODO: Implement In8AndFlag (TBIN8) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
