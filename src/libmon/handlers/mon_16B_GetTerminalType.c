/*
 * MON 16B (14 decimal): GetTerminalType (MGTTY)
 *
 * Gets the terminal type. The terminal type tells SINTRAN III how to handle a particular terminal. A wrong terminal type normally distorts the screen. The function-keys cannot be used.
 * 
 * - Appendix H lists the terminal types.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [O] TerminalType (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"

/* Terminal type reported to programs. Appendix H terminal types: 0 means an
 * ordinary/undefined terminal, which is the safe generic answer for an
 * emulated console (no ND-specific screen/function-key handling assumed). */
#define MON_TERMINAL_TYPE_GENERIC 0

MonResult mon_16B_GetTerminalType(MonContext* ctx) {
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_16B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* [O] TerminalType -> generic terminal (ND-500 INTEGER = 32-bit word) */
    mon_write_param_word(ctx, 1, MON_TERMINAL_TYPE_GENERIC);
    MON_LOG_OUT_WORD(ctx, 1, "TerminalType");

    mon_set_success(ctx);
    return MON_SUCCESS;
}
