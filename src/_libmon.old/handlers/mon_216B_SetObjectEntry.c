/*
 * MON 216B (142 decimal): SetObjectEntry (DWOBJ)
 *
 * Changes the description of a file. An object entry describes each file. It contains the file name, the access rights, the date it was last opened for read and write, the size, and more. You may use GetObjectEntry to read an object entry, change parts of it, then write it back with SetObjectEntry.
 * 
 * - You specify the directory index, the user index, and the object index.
 * - There is one object entry for each version of a file.
 * - Only user SYSTEM can change the object entry of a file without read or write access to the file.
 * - On the ND-100 you may access files on remote computer systems if the computers are connected through a COSMOS network.
 *
 * Parameters:
 *   [I] Buffer (INTEGER2[32]): input
 *   [I] DirIndex (INTEGER2): input
 *   [I] UserIndex (INTEGER2): input
 *   [I] ObjectIndex (INTEGER2): input
 *   [I] SystemId (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_216B_SetObjectEntry(MonContext* ctx) {
    /* TODO: Implement SetObjectEntry (DWOBJ) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Buffer");
    MON_LOG_IN_WORD(ctx, 1, "DirIndex");
    MON_LOG_IN_WORD(ctx, 2, "UserIndex");
    MON_LOG_IN_WORD(ctx, 3, "ObjectIndex");
    MON_LOG_IN_WORD(ctx, 4, "SystemId");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
