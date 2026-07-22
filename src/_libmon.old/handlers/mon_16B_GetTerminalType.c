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
#include "../mon_terminal_state.h"
#include <stdlib.h>

/* Terminal type is SINTRAN STATE, not a constant.
 *
 * On a real system it lives in SINTRAN's per-terminal datafield and is set with
 * the @SET-TERMINAL-TYPE command; 16B MGTTY only REPORTS it. 17B MSTTY and
 * 336B function 101B write it.
 *
 * 0 means NOT SET, and a program that needs VTM then ASKS the user:
 *
 *     Terminal type 000 is unknown.
 *     Available terminal types are: ...
 *     What is your terminal type?
 *
 * That is CORRECT SINTRAN behaviour, not a defect - do not "fix" it by inventing
 * a type in this handler.
 *
 * The emulator has no @SET-TERMINAL-TYPE yet (that belongs with the command
 * dispatcher, plan PART II phase 10), so the startup default comes from
 * ND500X_TERMINAL_TYPE, defaulting to 6. This is an emulator CHOICE, in the same
 * class as the pinned clock - it stands in for the @SET-TERMINAL-TYPE a real
 * operator would have run. Set ND500X_TERMINAL_TYPE=0 for authentic
 * "unset - ask me" behaviour.
 *
 * Why 6 (DEC VT100, 80 columns) as the default:
 *   - The console bytes reach a real host terminal, and host terminal emulators
 *     speak VT100.
 *   - The linker's own table, DDBTABLES-G06:VTM (extracted from vendor floppy
 *     ND-disk-00047.img), lists "6: DEC VT100 (80 columns)" in the menu it
 *     prints, so 6 is valid for the table we load.
 *   - CAVEAT: the type list DIFFERS BETWEEN DDBTABLES VARIANTS. G06 lists
 *     6/131/132/134/135; another observed variant jumps 3 -> 11 with no type 6.
 *     Type 2 ("Teletype ASR 33") appears in every variant seen.
 *   - The type->meaning mapping is NOT from a manual: Appendix H is not in the
 *     scanned document set. It is read from the DDBTABLES menu only.
 *
 * When the telnet server lands (plan PART II phase 9.3) this default should come
 * from the client's negotiated TERMINAL-TYPE instead of an env var. */
#define MON_TERMINAL_TYPE_DEFAULT 6   /* DEC VT100 (80 columns) */

static int32_t mon_terminal_type_startup_default(void) {
    const char* e = getenv("ND500X_TERMINAL_TYPE");
    if (e && *e) return (int32_t)strtol(e, NULL, 0);
    return MON_TERMINAL_TYPE_DEFAULT;
}

MonResult mon_16B_GetTerminalType(MonContext* ctx) {
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_16B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    uint32_t device_no = mon_read_param_word(ctx, 0);
    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    /* Device 0 means "own terminal" for a background program; device 1 is the
     * console (ND-860228.2 EN p505 notes). */
    if (device_no == 0) device_no = 1;

    /* First read for a device adopts the startup default, standing in for the
     * @SET-TERMINAL-TYPE an operator would have run. Once anything has SET the
     * type (17B MSTTY, 336B 101B), that value wins and round-trips. */
    int32_t type = mon_get_terminal_type(device_no);
    if (type == 0) {
        type = mon_terminal_type_startup_default();
        if (type != 0) mon_set_terminal_type(device_no, type);
    }

    /* [O] TerminalType (ND-500 INTEGER = 32-bit word) */
    mon_write_param_word(ctx, 1, (uint32_t)type);
    MON_LOG_OUT_WORD(ctx, 1, "TerminalType");

    mon_set_success(ctx);
    return MON_SUCCESS;
}
