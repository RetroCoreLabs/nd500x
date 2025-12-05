/*
 * MON 217B (143 decimal): GetAllFileIndexes (GUIOI)
 *
 * Gets the directory index, the user index, and the object index of a file. These are indexes in the SINTRAN III File System. Appendix C describes the File System.
 * 
 * - The file must be open.
 * - You may get the information if the file is on your local computer, or on a remote computer system if the computers are connected through a COSMOS network.
 *
 * Parameters:
 *   [I] FileNo (INTEGER): input
 *   [O] DirIndex (INTEGER): output
 *   [O] UserIndex (INTEGER): output
 *   [O] ObjectIndex (INTEGER): output
 *   [O] RemoteFlag (INTEGER): output
 *   [O] RemoteSystem (STRING): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_217B_GetAllFileIndexes(MonContext* ctx) {
    /* TODO: Implement GetAllFileIndexes (GUIOI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNo");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
