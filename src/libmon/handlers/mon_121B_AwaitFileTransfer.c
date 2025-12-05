/*
 * MON 121B (81 decimal): AwaitFileTransfer (WAITF)
 *
 * Checks that a data transfer to or from a mass-storage file is completed. The monitor call is relevant to ReadFromFile and WriteToFile operations. These data transfers are carried out independently of the CPU.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): input
 *   [I] ReturnFlag (INTEGER): input
 *   [O] Status (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_121B_AwaitFileTransfer(MonContext* ctx) {
    /* TODO: Implement AwaitFileTransfer (WAITF) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "ReturnFlag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
