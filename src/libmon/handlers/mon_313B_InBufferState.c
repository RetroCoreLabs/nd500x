/*
 * MON 313B (203 decimal): InBufferState (IBRISZ)
 *
 * Gets information about an input buffer. The current number of bytes in it, and the number of bytes until a break character, are returned.
 * 
 * - Use ExecutionInfo to get the logical device number for terminals. You can specify 1 for your own terminal.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *   [O] NoInBuffer (INTEGER): output
 *   [O] NoUntilBreak (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_file_table.h"

/*
 * Implemented against the carved contract (mon-analysis/313B-InBufferState):
 *   r->T (arg0) = logical device number; returns
 *     NoInBuffer   = characters currently held in the terminal input ring
 *     NoUntilBreak = characters before the next break (0 if no break)
 * Here the "terminal input ring" is our queued console input. The ND LINKER
 * calls IBRISZ(device=1, &NoInBuffer) (2-arg form) at startup to poll whether a
 * command is waiting. Report the remaining queued input length so the linker's
 * command reader proceeds instead of blocking. (device/break accounting is
 * modelled, not byte-exact - the worker link is uncarved; refine if needed.)
 */
MonResult mon_313B_InBufferState(MonContext* ctx) {
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    uint32_t remaining = (uint32_t)mon_get_console_input_remaining();

    /* arg1 = NoInBuffer (always present for the 2- and 3-arg forms) */
    if (ctx->arg_count >= 2) {
        mon_write_param_word(ctx, 1, remaining);
    }
    /* arg2 = NoUntilBreak (3-arg form). Our queued input is line-oriented and
     * terminated by CR, so a break is present whenever there is any input. */
    if (ctx->arg_count >= 3) {
        mon_write_param_word(ctx, 2, remaining);
    }

    /* Also return the count in W1. The ND LINKER reads its input-buffer length
     * from W1 after the IBRISZ CALLG (linker-b01.dom 0xB004DA8F -> W1 =: b.68 ->
     * used as the command-line length). Without this, W1 keeps the leftover
     * CALLG target address (0xF80000CB) and the linker's command parser walks
     * off the end and traps. */
    ctx->set_error_code(ctx->cpu, (int32_t)remaining);

    mon_log(MON_LOG_DEBUG, MON_ID_313B ": OUT: NoInBuffer=%u (args=%u, W1=%u)",
            remaining, ctx->arg_count, remaining);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
