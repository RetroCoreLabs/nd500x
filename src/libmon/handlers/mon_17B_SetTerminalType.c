/*
 * MON 17B (15 decimal): SetTerminalType (MSTTY)
 *
 * Sets the type of a terminal. The terminal type tells SINTRAN III how to handle a particular terminal. A wrong terminal type normally distorts the screen. The function keys cannot be used.
 * 
 * - Appendix H lists the terminal types.
 * - Public background users may only set the terminal type for their own terminal. A background program must be run from user SYSTEM or RT to set the terminal type for another terminal.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *   [I] TerminalType (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"

MonResult mon_17B_SetTerminalType(MonContext* ctx) {
    /* The emulated console has no ND-specific terminal-type behaviour to
     * configure, so accept and ignore the requested type. Success no-op. */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_17B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "TerminalType");

    mon_set_success(ctx);
    return MON_SUCCESS;
}
