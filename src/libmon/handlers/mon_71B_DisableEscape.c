/*
 * MON 71B (57 decimal): DisableEscape (DESCF)
 *
 * The ESCAPE key on the terminal normally terminates a program. This is called user break. This monitor call disables the escape function.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_terminal_state.h"

MonResult mon_71B_DisableEscape(MonContext* ctx) {
    /* TODO: Implement DisableEscape (DESCF) */

    /* DeviceNumber is ignored for background programs; own terminal is selected.
     * Device 1 == "own terminal". */
    uint32_t device_no = (ctx->arg_count >= 1) ? mon_read_param_word(ctx, 0) : 1;
    if (device_no == 0) device_no = 1;  /* own terminal */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Disable the ESCAPE (user-break) key on that terminal. Sets the terminal
     * datafield's 5IESC (inhibit-escape) bit - real per-device state, so a later
     * ESCAPE will not user-break until 72B EESCF re-enables it. */
    mon_set_escape_enabled(device_no, false);

    mon_log(MON_LOG_DEBUG, MON_ID_71B ": ESCAPE disabled on device %o", device_no);
    mon_set_success(ctx);
    return MON_SUCCESS;
}
