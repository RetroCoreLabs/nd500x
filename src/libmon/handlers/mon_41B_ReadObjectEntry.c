/*
 * MON 41B (33 decimal): ReadObjectEntry (ROBJE)
 *
 * Gets information about an opened file. An object entry describes each file. It contains the file name, the access rights, the date last opened for read and write, the size, and more. See the file system description in the SINTRAN III System Supervisor (ND-830003). You specify the file number.
 * 
 * - There is one object entry for each version of a file.
 * - The device number location in the object entry contains the logical device number and the unit number where the mass-storage file resides. The logical device number is placed in bit 11-0. The unit number in bit 15-12. The location contains the logical device number for peripheral files.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER2): input
 *   [O] Buff (BYTES[64]): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_41B_ReadObjectEntry(MonContext* ctx) {
    /* TODO: Implement ReadObjectEntry (ROBJE) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
