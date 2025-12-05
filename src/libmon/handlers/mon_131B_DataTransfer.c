/*
 * MON 131B (89 decimal): DataTransfer (ABSTR)
 *
 * Transfers data between physical memory and a mass-storage device, e.g. a disk or magnetic tape. You may perform various device control functions. This monitor call is mainly used by the operating system itself. For more details, refer to the ND-100 SCSI Reference Guide, ND-12.048.
 * 
 * - With SINTRAN III/VSX, the monitor call and the parameters must reside on a fixed segment on protection ring 2. With SINTRAN III/VSE, the monitor call and parameters must reside in resident memory.
 * - The physical memory area must be contiguous. Older versions of magnetic tapes or disk controllers cannot cross physical memory bank boundaries of 128 Kbytes. These magnetic tapes have ND numbers less than ND-537. The disks have ND numbers less than ND-559.
 * - If you write code to be independent of whether it is run on SINTRAN III VSE or VSX, you are advised to use TransferData (EXABS, mon 335).
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): input
 *   [I] Func (INTEGER): input
 *   [I] MemoryAddr (INTEGER): input
 *   [I] BlockAddr (INTEGER): input
 *   [I] NoOfBlocks (INTEGER): input
 *   [O] Stat (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_131B_DataTransfer(MonContext* ctx) {
    /* TODO: Implement DataTransfer (ABSTR) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "Func");
    MON_LOG_IN_WORD(ctx, 2, "MemoryAddr");
    MON_LOG_IN_WORD(ctx, 3, "BlockAddr");
    MON_LOG_IN_WORD(ctx, 4, "NoOfBlocks");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
