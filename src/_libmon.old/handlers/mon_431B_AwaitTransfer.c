/*
 * MON 431B (281 decimal): AwaitTransfer (MWAITF)
 *
 * Checks that a data transfer to or from a mass-storage file is completed. The monitor call is relevant to DeviceFunction, ReadFromFile and WriteToFile operations. These are carried out independently of the CPU. The number of bytes read or written is returned.
 * 
 * - You may specify that the program should wait if the transfer is not ready. It is set in the I/O wait state.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER2): input
 *   [I] WaitFlag (INTEGER2): input
 *   [O] NoOfBytes (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_431B_AwaitTransfer(MonContext* ctx) {
    /* TODO: Implement AwaitTransfer (MWAITF) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "WaitFlag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
