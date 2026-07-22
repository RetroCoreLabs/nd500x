/*
 * MON 274B (188 decimal): GetFileIndexes (FOBJN)
 *
 * Gets the directory index, the user index, and the object index of a file. These are indexes in the file system. See the SINTRAN III System Supervisor {ND-830003} for more details.
 * 
 * - The file need not be open.
 *
 * Parameters:
 *   [I] FileName (STRING): input
 *   [I] FileType (STRING): input
 *   [O] DirIndex (INTEGER2): output
 *   [O] UserIndex (INTEGER2): output
 *   [O] ObjectIndex (INTEGER2): output
 *   [O] NextObjectIndex (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_274B_GetFileIndexes(MonContext* ctx) {
    /* TODO: Implement GetFileIndexes (FOBJN) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileName");
    MON_LOG_IN_WORD(ctx, 1, "FileType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
