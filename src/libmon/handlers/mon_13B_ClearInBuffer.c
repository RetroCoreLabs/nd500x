/*
 * MON 13B (11 decimal): ClearInBuffer (CIBUF)
 *
 * Clears a device input buffer. Input from character devices, e.g. terminals, are temporarily stored in this buffer.
 * 
 * - You can use logical device number 1 for your own terminal in background programs.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"

MonResult mon_13B_ClearInBuffer(MonContext* ctx) {
    /* CIBUF discards any UNREAD, type-ahead input pending in a character
     * device's buffer. The emulator has no separate hardware type-ahead
     * buffer to flush, and the queued-console input models the program's
     * OWN scripted command stream - clearing that would drop commands the
     * program still intends to read. So this is a success no-op: there is
     * no pending device-buffer state to clear. */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, MON_ID_13B ": Missing parameters (need 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    mon_set_success(ctx);
    return MON_SUCCESS;
}
