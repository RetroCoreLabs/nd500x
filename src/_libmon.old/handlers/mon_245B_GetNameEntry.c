/*
 * MON 245B (165 decimal): GetNameEntry (GNAEN)
 *
 * Gets information about devices, e.g. disks and floppy disks. The monitor call returns the name entry of a device. You specify the name index.
 * 
 * - GetDirNameIndex provides the name index.
 *
 * Parameters:
 *   [I] NameIndex (INTEGER): input
 *   [O] NameTableEntry (ARRAY): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_245B_GetNameEntry(MonContext* ctx) {
    /* TODO: Implement GetNameEntry (GNAEN) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "NameIndex");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
