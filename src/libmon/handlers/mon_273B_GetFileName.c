/*
 * MON 273B (187 decimal): GetFileName (MGFIL)
 *
 * Gets the name of a file. You specify the directory index, the user index, and the object index. The file name, the file type, and the version are returned.
 * 
 * - The file need not be open.
 * - On the ND-100 you may specify a file on a remote computer system. The computers must be connected through a COSMOS network.
 *
 * Parameters:
 *   [I] DirIndex (INTEGER): input
 *   [I] UserIndex (INTEGER): input
 *   [I] ObjectIndex (INTEGER): input
 *   [O] FileName (STRING): output
 *   [I] RemoteFlag (INTEGER): input
 *   [IO] RemoteSystem (STRING): in/out
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_273B_GetFileName(MonContext* ctx) {
    /* TODO: Implement GetFileName (MGFIL) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "DirIndex");
    MON_LOG_IN_WORD(ctx, 1, "UserIndex");
    MON_LOG_IN_WORD(ctx, 2, "ObjectIndex");
    MON_LOG_IN_WORD(ctx, 4, "RemoteFlag");
    MON_LOG_IN_WORD(ctx, 5, "RemoteSystem");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
