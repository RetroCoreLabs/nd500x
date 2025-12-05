/*
 * MON 276B (190 decimal): EnableLocal (ELOFU)
 *
 * You may log in on remote computers through the COSMOS data network. A key on the terminal returns you to your local computer. This local function can be disabled. You enable it again with EnableLocal.
 *
 * Parameters:
 *   [I] ProgramAddress (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_276B_EnableLocal(MonContext* ctx) {
    /* TODO: Implement EnableLocal (ELOFU) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "ProgramAddress");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
