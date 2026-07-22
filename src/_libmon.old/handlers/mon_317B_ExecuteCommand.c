/*
 * MON 317B [UECOM/ExecuteCommand]
 *
 * Executes a SINTRAN III command. Specify the command name and the parameters
 * as a text string.
 *
 * - An error message is output if an error occurs. The program does not terminate.
 * - Some commands may destroy your program.
 * - Some commands have output, e.g. @LIST-FILES. This is displayed on the terminal.
 *
 * Parameters:
 *   [I] Command (STRING): SINTRAN III command to execute
 *
 * Note: This is a stub implementation that logs the command but doesn't execute it.
 * SINTRAN III command execution is not implemented.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include <stdlib.h>

MonResult mon_317B_ExecuteCommand(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, MON_ID_317B ": Missing parameters (need 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* TEMP probe: dump the raw command descriptor and the memory it points at,
     * to map NC's UECOM argument layout (the CAT-500 invocation). */
    if (getenv("ND500X_UECOM_DUMP")) {
        uint32_t da = ctx->arg_addresses[0];
        mon_log(MON_LOG_WARN, MON_ID_317B ": arg0 desc @0x%08X", da);
        for (int w = 0; w < 4; w++) {
            uint32_t v = ctx->read_word(ctx->cpu, da + (uint32_t)(w*4));
            mon_log(MON_LOG_WARN, "   desc[%d] @0x%08X = 0x%08X", w, da + (uint32_t)(w*4), v);
        }
        /* Dump 64 bytes at the descriptor address itself (in case it IS the text) */
        char raw[65];
        for (int i = 0; i < 64; i++) {
            uint8_t b = ctx->read_byte(ctx->cpu, da + (uint32_t)i);
            raw[i] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
        }
        raw[64] = 0;
        mon_log(MON_LOG_WARN, "   bytes@desc: \"%s\"", raw);
        /* Dump what each descriptor word points at (candidates for the command text) */
        for (int w = 0; w < 3; w++) {
            uint32_t p = ctx->read_word(ctx->cpu, da + (uint32_t)(w*4));
            if (!p) continue;
            for (int i = 0; i < 64; i++) {
                uint8_t b = ctx->read_byte(ctx->cpu, p + (uint32_t)i);
                raw[i] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
            }
            raw[64] = 0;
            mon_log(MON_LOG_WARN, "   *desc[%d]=0x%08X: \"%s\"", w, p, raw);
        }
    }

    /* Read command string. NC passes the command as a [Length:4][Pointer:4]
     * descriptor (arg0 = {maxlen, cmd_ptr, ...}; the command text lives at the
     * pointer and is 0x27-terminated), e.g. "NC-A", "CAT-CAT5-B". Try that
     * descriptor form first; fall back to a direct read for callers that pass
     * the string inline. */
    char command[256];
    if (mon_read_descriptor_string(ctx, 0, command, sizeof(command)) <= 0) {
        mon_read_sintran_string(ctx, 0, command, sizeof(command));
    }

    mon_log(MON_LOG_INFO, MON_ID_317B ": IN: Command='%s'", command);

    /* Command execution (nested subsystem invocation, e.g. the CAT-500 back-end
     * "CAT-CAT5-B") is not yet performed here; see the CAT-500 route work. The
     * command is now decoded correctly so callers see a valid name. */

    mon_set_success(ctx);
    return MON_SUCCESS;
}
