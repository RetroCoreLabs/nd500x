/*
 * MON 214B (140 decimal): GetUserName (GUSNA)
 *
 * Gets the name of a user. The user may be on a remote computer if the COSMOS network is installed. The remote system name is then returned.
 * 
 * - RT programs return the name of user RT.
 *
 * Parameters:
 *   [O] UserName (STRING): output
 *   [I] DirectoryIndex (INTEGER): input
 *   [I] UserIndex (INTEGER): input
 *   [O] RemoteFlag (INTEGER): output
 *   [O] RemoteSystem (STRING): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_config.h"
#include "../mon_errors.h"
#include <string.h>

/* Carve-verified contract (006-S3FS:105301B): returns the name of the user
 * identified by (DirectoryIndex, UserIndex) into a 16-char output buffer, plus a
 * RemoteFlag (0 = local). The ND LINKER uses it in a two-pass file search
 * (current user, then (SYSTEM)). We return the emulator's current user name
 * (space-padded to 16 chars), RemoteFlag = 0. */
#define GUSNA_NAME_LEN 16

MonResult mon_214B_GetUserName(MonContext* ctx) {
    if (ctx->arg_count < 3) {
        mon_log(MON_LOG_WARN, MON_ID_214B ": Missing parameters (need >=3, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    MON_LOG_IN_WORD(ctx, 1, "DirectoryIndex");
    MON_LOG_IN_WORD(ctx, 2, "UserIndex");

    const char* user = mon_config_get_current_user();
    if (!user) user = "GUEST";

    /* Write the user name to arg0, upper-cased and SPACE-padded to 16 chars
     * (SINTRAN string convention - no NUL terminator, blank-filled). */
    uint32_t name_addr = ctx->arg_addresses[0];
    size_t ulen = strlen(user);
    for (int i = 0; i < GUSNA_NAME_LEN; i++) {
        char c = (i < (int)ulen) ? user[i] : ' ';
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        ctx->write_byte(ctx->cpu, name_addr + (uint32_t)i, (uint8_t)c);
    }

    /* RemoteFlag = 0 (local user) */
    if (ctx->arg_count >= 4) {
        mon_write_param_word(ctx, 3, 0);
    }

    mon_log(MON_LOG_DEBUG, MON_ID_214B ": OUT: UserName='%s' (local)", user);
    mon_set_success(ctx);
    return MON_SUCCESS;
}
