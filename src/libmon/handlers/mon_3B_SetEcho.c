/*
 * MON 3B (3 decimal): SetEcho (ECHOM)
 *
 * Sets the echo mode for a terminal. Echo controls whether characters
 * typed are displayed back to the user during input operations.
 *
 * Echo Strategy Values:
 *   <0: No echo - characters not displayed
 *    0: Echo all characters
 *    1: Echo except control characters (0-31)
 *    2: MAC echo strategy - echo printable + CR/LF
 *  3-6: System-defined tables
 *    7: User-defined 128-bit table (Table parameter)
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): Logical device number
 *   [I] EchoStrategy (INTEGER): Echo strategy code
 *   [I] Table (ARRAY): 128-bit user echo table (4 words, strategy 7 only)
 *
 * Returns:
 *   Success: K flag clear
 *   Error: K flag set, error code in I1
 *
 * Notes:
 *   - If 8-bit I/O is set (TerminalFunction, function number 112),
 *     you will get echo always if bit 7 in the echo table is set.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_terminal_state.h"

MonResult mon_3B_SetEcho(MonContext* ctx) {
    /* Need at least DeviceNumber and EchoStrategy */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_3B ": Missing parameters (need at least 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t device_no = mon_read_param_word(ctx, 0);
    int32_t strategy = (int32_t)mon_read_param_word(ctx, 1);

    /* Read user table if strategy is 7 (user-defined) */
    uint32_t user_table[4] = {0};
    if (strategy == ECHO_STRAT_USER && ctx->arg_count >= 3) {
        /* Table is passed as array address - read 4 words (128 bits) */
        uint32_t table_addr = ctx->arg_addresses[2];
        for (int i = 0; i < 4; i++) {
            user_table[i] = ctx->read_word(ctx->cpu, table_addr + i * 4);
        }
    }

    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "EchoStrategy");

    mon_log(MON_LOG_DEBUG, MON_ID_3B ": IN: DeviceNo=%u (%o), Strategy=%d",
            device_no, device_no, strategy);

    /* For strategy 7, log the user table */
    if (strategy == ECHO_STRAT_USER) {
        mon_log(MON_LOG_DEBUG, MON_ID_3B ":     UserTable=[%08X %08X %08X %08X]",
                user_table[0], user_table[1], user_table[2], user_table[3]);
    }

    /* Apply the strategy */
    int result = mon_set_echo_strategy(device_no, strategy,
                                       (strategy == ECHO_STRAT_USER) ? user_table : NULL);

    if (result < 0) {
        mon_set_error(ctx, (uint32_t)(-result));
        return MON_ERROR;
    }

    mon_log(MON_LOG_INFO, MON_ID_3B ": OUT: Device %u echo strategy set to %d",
            device_no, strategy);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
