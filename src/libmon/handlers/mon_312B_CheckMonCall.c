/*
 * MON 312B (202 decimal): CheckMonCall (MOINF)
 *
 * Some monitor calls are optional or only available in later versions of
 * SINTRAN III. This monitor call checks if a monitor call exists in your
 * particular SINTRAN III system.
 *
 * Parameters:
 *   [I] MonCallNumber (INTEGER): MON call number to check (decimal)
 *   [O] MonCallEntry (INTEGER): Returns 0 if exists, -1 if not (in W1)
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"

MonResult mon_312B_CheckMonCall(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, "MON 312B MOINF: Missing parameters (need 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read MON call number to check */
    uint32_t mon_number = mon_read_param_word(ctx, 0);

    MON_LOG_IN_WORD(ctx, 0, "MonCallNumber");

    mon_log(MON_LOG_DEBUG, "MON 312B MOINF: Checking MON %u (%oB)", mon_number, mon_number);

    /* Check if the MON call is implemented */
    MonImplStatus status = mon_get_status(mon_number);

    int32_t result;
    if (status == MON_STATUS_VALIDATED || status == MON_STATUS_IN_PROGRESS) {
        /* MON call exists */
        result = 0;
        mon_log(MON_LOG_DEBUG, "MON 312B MOINF: MON %u exists (status=%d)", mon_number, status);
    } else {
        /* MON call does not exist */
        result = -1;
        mon_log(MON_LOG_DEBUG, "MON 312B MOINF: MON %u does not exist", mon_number);
    }

    /* Return result in W1 */
    ctx->set_error_code(ctx->cpu, (uint32_t)result);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
