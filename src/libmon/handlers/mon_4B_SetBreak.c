/*
 * MON 4B (4 decimal): SetBreak (BRKM)
 *
 * Sets the break characters for a terminal. Normally, a program waits for input. When a break character is typed, the program restarts. For example, most subsystems restart when you press the RETURN-key after a command. The subsystems have defined the RETURN-key as a break character.
 * 
 * - SINTRAN III has some predefined break tables.
 * - You may define your own break table. This is a 128-bit array where each bit represents an ASCII character. Use 1 for the characters you want as break characters. The ability to define your own break tables is optional in older versions of SINTRAN III.
 * - If 8-bit I/O is set (TerminalFunction, function number 112), you will get break always if bit 7 in the break table is set.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): input
 *   [I] BreakStrategy (INTEGER): input
 *   [I] Table (ARRAY): input
 *   [I] NoOfChar (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_4B_SetBreak(MonContext* ctx) {
    /* TODO: Implement SetBreak (BRKM) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "BreakStrategy");
    MON_LOG_IN_WORD(ctx, 2, "Table");
    MON_LOG_IN_WORD(ctx, 3, "NoOfChar");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
