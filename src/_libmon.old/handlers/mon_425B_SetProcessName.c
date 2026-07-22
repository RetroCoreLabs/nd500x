/*
 * MON 425B (277 decimal): SetProcessName (SPRNAM)
 *
 * Defines a new name for your process.
 * 
 * - Process names may be up to 16 characters and contain an additional user name, e.g. (P-HANSEN)WP-PROCESS.
 *
 * Parameters:
 *   [I] ProcessName (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_425B_SetProcessName(MonContext* ctx) {
    /* TODO: Implement SetProcessName (SPRNAM) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "ProcessName");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
