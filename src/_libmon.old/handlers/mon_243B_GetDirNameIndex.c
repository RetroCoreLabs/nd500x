/*
 * MON 243B (163 decimal): GetDirNameIndex (FDINA)
 *
 * Gets directory index and name index. The name index identifies the device description of the disk. You have to specify the directory name.
 *
 * Parameters:
 *   [I] DirName (STRING): input
 *   [O] DirIndex (INTEGER): output
 *   [O] NameIndex (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_243B_GetDirNameIndex(MonContext* ctx) {
    /* TODO: Implement GetDirNameIndex (FDINA) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DirName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
