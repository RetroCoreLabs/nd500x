/*
 * MON 72B (58 decimal): EnableEscape (EESCF)
 *
 * Enables the ESCAPE key on the terminal. The ESCAPE key normally terminates a program. This is called user break. You can disable this key with DisableEscape. To enable it again you should use EnableEscape.
 * 
 * - The escape function is enabled when you log out.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_terminal_state.h"

MonResult mon_72B_EnableEscape(MonContext* ctx) {
    /* TODO: Implement EnableEscape (EESCF) */

    uint32_t device_no = (ctx->arg_count >= 1) ? mon_read_param_word(ctx, 0) : 1;
    if (device_no == 0) device_no = 1;  /* own terminal */
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Enable the ESCAPE (user-break) key - inverse of 71B DESCF. Clears the
     * terminal datafield's 5IESC bit so ESCAPE resumes user-breaking. */
    mon_set_escape_enabled(device_no, true);

    mon_log(MON_LOG_DEBUG, MON_ID_72B ": ESCAPE enabled on device %o", device_no);
    mon_set_success(ctx);
    return MON_SUCCESS;
}
