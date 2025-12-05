/*
 * MON 236B (158 decimal): SetPermanentOpen (SPERD)
 *
 * Sets a file permanently open. The file is not closed by CloseFile with -1 as file number. You have to specify the file number or -2.
 * 
 * - The file must already be open.
 * - Only mass-storage files can be set permanently open.
 * - The file is not closed when your program terminates.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_236B_SetPermanentOpen(MonContext* ctx) {
    /* TODO: Implement SetPermanentOpen (SPERD) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
