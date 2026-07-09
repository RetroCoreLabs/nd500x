/*
 * MON 312B [MOINF/CheckMonCall]
 *
 * Some monitor calls are optional or only available in later versions of
 * SINTRAN III. This monitor call checks if a monitor call exists in your
 * particular SINTRAN III system.
 *
 * Parameters:
 *   [I] MonCallNumber (INTEGER): MON call number to check
 *   [O] MonCallEntry (INTEGER): Address of entry (0 if exists, non-zero if not)
 *
 * Note: Result is written to OUTPUT parameter, NOT W1.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"

MonResult mon_312B_CheckMonCall(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, MON_ID_312B ": Missing parameters (need 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read MON call number to check */
    uint32_t mon_number = mon_read_param_word(ctx, 0);

    MON_LOG_IN_WORD(ctx, 0, "MonCallNumber");

    mon_log(MON_LOG_DEBUG, MON_ID_312B ": IN: MonCallNumber=%oB", mon_number);

    /* Check if the MON call is implemented */
    MonImplStatus status = mon_get_status(mon_number);

    uint32_t result;
    if (status == MON_STATUS_VALIDATED || status == MON_STATUS_IN_PROGRESS) {
        /* MON call exists - return non-zero (fake entry address) */
        result = 0xF8000000 + mon_number;  /* Segment 31 + MON number */
        mon_log(MON_LOG_DEBUG, MON_ID_312B ": OUT: MON %oB exists (entry=0x%08X)", mon_number, result);
    } else {
        /* NOT_IMPLEMENTED and DEPRECATED both report as non-existent (0) */
        result = 0;
        mon_log(MON_LOG_DEBUG, MON_ID_312B ": OUT: MON %oB does not exist", mon_number);
    }

    /* Write result to OUTPUT parameter (arg[1]), not W1 */
    if (ctx->arg_count >= 2) {
        mon_write_param_word(ctx, 1, (uint32_t)result);
        MON_LOG_OUT_WORD(ctx, 1, "MonCallEntry");
    }

    mon_set_success(ctx);
    return MON_SUCCESS;
}
