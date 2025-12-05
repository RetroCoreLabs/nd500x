/*
 * MON 165B (117 decimal): GetInRegisters (DIW)
 *
 * Reads the device interface registers.
 *
 * Parameters:
 *   [I] NoOfReg (INTEGER): input
 *   [I] Buffer (ARRAY): input
 *   [I] DataBuffer (ARRAY): input
 *   [O] ErrorIndicator (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_165B_GetInRegisters(MonContext* ctx) {
    /* TODO: Implement GetInRegisters (DIW) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "NoOfReg");
    MON_LOG_IN_WORD(ctx, 1, "Buffer");
    MON_LOG_IN_WORD(ctx, 2, "DataBuffer");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
