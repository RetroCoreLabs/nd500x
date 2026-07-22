/*
 * MON 31B (25 decimal): IOInstruction (EXIOX)
 *
 * Executes an IOX machine instruction. The IOX instruction handles the device registers. The IOX instruction must be inserted in the IOX table by the SINTRAN-SERVICE-PROGRAM command INSERT-IN-IOX-TABLE first.
 * 
 * - SINTRAN III must know the device register addresses.
 * - This monitor call may be used in debugging of device interfaces.
 *
 * Parameters:
 *   [I] RegContents (INTEGER2): input
 *   [I] DevRegAddr (INTEGER2): input
 *   [O] ContentsAfter (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_31B_IOInstruction(MonContext* ctx) {
    /* TODO: Implement IOInstruction (EXIOX) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RegContents");
    MON_LOG_IN_WORD(ctx, 1, "DevRegAddr");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
