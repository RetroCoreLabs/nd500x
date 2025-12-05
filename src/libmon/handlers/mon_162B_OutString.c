/*
 * MON 162B (114 decimal): OutString (OUTST)
 *
 * Writes a string of characters to a peripheral file, e.g., a terminal or a printer.
 * 
 * - You cannot use this monitor call for mass-storage files.
 * - The output buffer of the device may be too small. Then the program waits until the required buffer space becomes available.
 * - Parameters are fetched and returned through the alternative page table.
 * - The maximum string length is 2048 bytes (as in OutputString).
 * - For performance reasons, it is inadvisable to use this call from the ND-500(0). Use OutputString instead.
 * - Appendix F contains an ASCII table.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER2): input
 *   [I] TextWrite (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_162B_OutString(MonContext* ctx) {
    /* TODO: Implement OutString (OUTST) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "TextWrite");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
