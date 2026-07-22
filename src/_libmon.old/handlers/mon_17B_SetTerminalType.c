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
#include "../mon_terminal_state.h"

MonResult mon_17B_SetTerminalType(MonContext* ctx) {
    /* This is SINTRAN's @SET-TERMINAL-TYPE mechanism at the MON level: MSTTY
     * writes the per-terminal type, 16B MGTTY reads it back. It used to accept
     * and ignore, so set->get did not round-trip and a program could not
     * configure its own terminal - which matters because a program that reads
     * type 0 (not set) asks the user instead. */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_17B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t type      = mon_read_param_word(ctx, 1);
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "TerminalType");

    if (device_no == 0) device_no = 1;  /* own terminal */
    mon_set_terminal_type(device_no, (int32_t)type);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
