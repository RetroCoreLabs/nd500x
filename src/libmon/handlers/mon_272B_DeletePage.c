/*
 * MON 272B (186 decimal): DeletePage (DELPG)
 *
 * Deletes pages from a file. Pages between two page numbers are removed.
 * 
 * - The file must be opened.
 *
 * Parameters:
 *   [I] FileNo (INTEGER2): input
 *   [I] FirstPage (INTEGER2): input
 *   [I] LastPage (INTEGER2): input
 *   [O] NoOfPages (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_272B_DeletePage(MonContext* ctx) {
    /* TODO: Implement DeletePage (DELPG) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNo");
    MON_LOG_IN_WORD(ctx, 1, "FirstPage");
    MON_LOG_IN_WORD(ctx, 2, "LastPage");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
