/*
 * MON 4B (4 decimal): SetBreak (BRKM)
 *
 * Sets the break characters for a terminal. Break characters terminate
 * input operations in DVINST (MON 503B) and similar calls.
 *
 * Break Strategy Values:
 *   <0: No break - input continues until max chars or EOF
 *    0: All characters break - every character terminates input
 *    1: Control characters (0-31) break
 *    2: MAC (machine code) - CR, LF, ESC, EOF, 0x27
 *  3-6: System-defined tables
 *    7: User-defined 128-bit table (Table parameter)
 *    8: Last user-defined table (don't resend table)
 *    9: Max chars only - break only when NoOfChar reached
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): Logical device number
 *   [I] BreakStrategy (INTEGER): Break strategy code
 *   [I] Table (ARRAY): 128-bit user break table (4 words, strategy 7 only)
 *   [I] NoOfChar (INTEGER): Max chars before auto-break
 *
 * Returns:
 *   Success: K flag clear
 *   Error: K flag set, error code in I1
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_errors.h"
#include "../mon_terminal_state.h"

MonResult mon_4B_SetBreak(MonContext* ctx) {
    /* Need at least DeviceNo and BreakStrategy */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_4B ": Missing parameters (need at least 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t device_no = mon_read_param_word(ctx, 0);
    int32_t strategy = (int32_t)mon_read_param_word(ctx, 1);

    /* Read user table if strategy is 7 (user-defined) */
    uint32_t user_table[4] = {0};
    if (strategy == BREAK_STRAT_USER && ctx->arg_count >= 3) {
        /* Table is passed as array address - read 4 words (128 bits) */
        uint32_t table_addr = ctx->arg_addresses[2];
        for (int i = 0; i < 4; i++) {
            user_table[i] = ctx->read_word(ctx->cpu, table_addr + i * 4);
        }
    }

    /* Read max chars parameter */
    uint32_t max_chars = 0;
    if (ctx->arg_count >= 4) {
        max_chars = mon_read_param_word(ctx, 3);
    }

    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "BreakStrategy");

    mon_log(MON_LOG_DEBUG, MON_ID_4B ": IN: DeviceNo=%u (%o), Strategy=%d, MaxChars=%u",
            device_no, device_no, strategy, max_chars);

    /* For strategy 7, log the user table */
    if (strategy == BREAK_STRAT_USER) {
        mon_log(MON_LOG_DEBUG, MON_ID_4B ":     UserTable=[%08X %08X %08X %08X]",
                user_table[0], user_table[1], user_table[2], user_table[3]);
    }

    /* Apply the strategy */
    int result = mon_set_break_strategy(device_no, strategy,
                                        (strategy == BREAK_STRAT_USER) ? user_table : NULL,
                                        max_chars);

    if (result < 0) {
        mon_set_error(ctx, (uint32_t)(-result));
        return MON_ERROR;
    }

    mon_log(MON_LOG_INFO, MON_ID_4B ": OUT: Device %u break strategy set to %d",
            device_no, strategy);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
