/*
 * MON 235B (157 decimal): ScratchOpen (SCROP)
 *
 * Opens a file as a scratch file. A maximum of 64 pages of the file is kept when you close the file. Use SET-CLOSED-FILE-SIZE in the SINTRAN-SERVICE-PROGRAM to change this.
 *
 * Parameters:
 *   [O] FileNo (INTEGER): output
 *   [I] AccessCode (INTEGER): input
 *   [I] FileName (STRING): input
 *   [I] FileType (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_235B_ScratchOpen(MonContext* ctx) {
    /* TODO: Implement ScratchOpen (SCROP) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 1, "AccessCode");
    MON_LOG_IN_WORD(ctx, 2, "FileName");
    MON_LOG_IN_WORD(ctx, 3, "FileType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
