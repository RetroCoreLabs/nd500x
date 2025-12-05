/*
 * MON 215B (141 decimal): GetObjectEntry (DROBJ)
 *
 * Gets information about a file. An object entry describes each file. It contains the file name, the access rights, the date last opened for read and write, the size, and more. See the file system description in appendix C. You specify the directory index, the user index, and the object index.
 * 
 * - There is one object entry for each version of a file.
 * - Only user SYSTEM can get the object entry of a file without read or write access to the file.
 * - You may access files on remote computer systems. The computers must be connected through a COSMOS network.
 *
 * Parameters:
 *   [O] Buffer (INTEGER2[32]): output
 *   [I] DirIndex (INTEGER2): input
 *   [I] UserIndex (INTEGER2): input
 *   [I] ObjectIndex (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_215B_GetObjectEntry(MonContext* ctx) {
    /* TODO: Implement GetObjectEntry (DROBJ) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 1, "DirIndex");
    MON_LOG_IN_WORD(ctx, 2, "UserIndex");
    MON_LOG_IN_WORD(ctx, 3, "ObjectIndex");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
