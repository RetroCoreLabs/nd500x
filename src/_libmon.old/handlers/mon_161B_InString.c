/*
 * MON 161B (113 decimal): InString (INSTR)
 *
 * Reads a string of characters from a peripheral device, e.g. a terminal.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): input
 *   [O] TextRead (STRING): output
 *   [I] NoOfBytes (INTEGER): input
 *   [I] Terminator (INTEGER): input
 *   [O] ReturnStatus (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_161B_InString(MonContext* ctx) {
    /* TODO: Implement InString (INSTR) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 2, "NoOfBytes");
    MON_LOG_IN_WORD(ctx, 3, "Terminator");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
