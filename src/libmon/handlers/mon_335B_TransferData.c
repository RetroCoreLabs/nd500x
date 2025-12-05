/*
 * MON 335B (221 decimal): TransferData (EXABS)
 *
 * Transfers data between physical memory and a mass-storage device, e.g. a disk. You may perform various device control functions. This monitor call is mainly used by the operating system itself.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [I] Func (INTEGER): input
 *   [I] MemAddr (INTEGER): input
 *   [I] BlockAddr (INTEGER): input
 *   [IO] NoOfBlocks (INTEGER): in/out
 *   [O] ReturnStatus (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_335B_TransferData(MonContext* ctx) {
    /* TODO: Implement TransferData (EXABS) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "Func");
    MON_LOG_IN_WORD(ctx, 2, "MemAddr");
    MON_LOG_IN_WORD(ctx, 3, "BlockAddr");
    MON_LOG_IN_WORD(ctx, 4, "NoOfBlocks");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
