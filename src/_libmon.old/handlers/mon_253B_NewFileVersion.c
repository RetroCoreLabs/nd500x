/*
 * MON 253B (171 decimal): NewFileVersion (CRALN)
 *
 * Creates new versions of a file. You may create new versions for both indexed, contiguous and allocated files.
 * 
 * - You must have directory access to the user area where you create the file. User SYSTEM and RT get the owners access rights.
 * - The number following the semicolon in a file name is the version number. For example, TEST:SYMB:4 version 4 of the file.
 * - The file must exist in advance.
 * - Use DeleteFile to delete file versions.
 *
 * Parameters:
 *   [I] FileName (STRING): input
 *   [I] FirstPage (INTEGER): input
 *   [I] NoOfPages (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_253B_NewFileVersion(MonContext* ctx) {
    /* TODO: Implement NewFileVersion (CRALN) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileName");
    MON_LOG_IN_WORD(ctx, 1, "FirstPage");
    MON_LOG_IN_WORD(ctx, 2, "NoOfPages");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
