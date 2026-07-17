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

/* Terminal type reported to programs.
 *
 * This used to return 0 on the assumption that 0 meant "ordinary/undefined
 * terminal" and was the safe generic answer. That assumption is DISPROVEN: the
 * ND linker rejects it outright, printing
 *
 *     Terminal type 000 is unknown.
 *     Available terminal types are: ...
 *     What is your terminal type?
 *
 * and then loops on the prompt. 0 is not a valid type.
 *
 * 6 = "DEC VT100 (80 columns)". The console bytes end up in a real host
 * terminal, and every host terminal emulator in practical use speaks VT100 - so
 * reporting VT100 describes what is actually on the other end of the stream.
 * (A duller answer such as 2 "Teletype ASR 33" would be safe but would deny the
 * guest cursor addressing and function keys that the host can honour.)
 *
 * EVIDENCE, and its limits - the type->meaning mapping is NOT read from a
 * manual: Appendix H, which lists the terminal types, is not in the scanned
 * document set. What we have instead:
 *   - The linker's own terminal table, DDBTABLES-G06:VTM (extracted from vendor
 *     floppy ND-disk-00047.img), lists "6: DEC VT100 (80 columns)" in the menu
 *     it prints at startup. That menu is generated from that file, so 6 is
 *     valid for the table we actually load.
 *   - CAVEAT: the type list DIFFERS BETWEEN DDBTABLES VARIANTS. G06 lists
 *     6/131/132/134/135; another observed variant jumps 3 -> 11 and has NO type
 *     6 at all (it lists 11/36/52/57/79/91/92/99/105 instead). Under such a
 *     variant this answer would be rejected the same way 0 is today. Type 2
 *     ("Teletype ASR 33") appears in every variant seen and is the fallback if
 *     that ever bites.
 *
 * This is an emulator CHOICE, in the same class as the pinned clock - it is not
 * a claim about what SINTRAN would report for real hardware. */
#define MON_TERMINAL_TYPE_GENERIC 6   /* DEC VT100 (80 columns) */

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
