/*
 * MON 252B (170 decimal): BackupClose (BCLOS)
 *
 * Closes a file. The version number and the last date accessed are unchanged. The number of pages in temporary files and spooling files is not affected.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): input
 *   [I] Flag (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_252B_BackupClose(MonContext* ctx) {
    /* TODO: Implement BackupClose (BCLOS) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "Flag");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
