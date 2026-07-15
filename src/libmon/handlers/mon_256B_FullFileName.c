/*
 * MON 256B [DEABF/FullFileName]
 *
 * Returns a complete file name from an abbreviated one. The directory, the user,
 * the file name, the file type, and the version are returned.
 *
 * Parameters (ND-500 CALLG form - the default-file-type param 3 exists only on
 * the ND-100 form and is IGNORED on the ND-500; NC/linker call with 2 args):
 *   [I] AbbrevFileName (STRING): Abbreviated file name
 *   [O] FileName (STRING): Full file name output (apostrophe-terminated)
 *   [I] FileType (STRING, optional/ND-100 only): Default file type
 *
 * Return contract (carve 006-S3FS DEABF + NC-oracle tier3): on a name that
 * resolves to an existing file, write the expanded name and return SUCCESS; on
 * an unresolved name, set the K flag and return error 46 (056B NO SUCH FILE
 * NAME) - the exact code NC tests to decide whether to CREATE the file. A
 * pass-through echo that always succeeds silently breaks NC's create-if-missing.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_path.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

MonResult mon_256B_FullFileName(MonContext* ctx) {
    /* ND-500 form takes 2 args (AbrevName, FullName). The default-file-type
     * (arg 3) is ND-100-only and ignored here; require only 2 args so NC's real
     * 2-arg CALLG is not rejected. */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_256B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read input strings. FileType (arg 2) is ND-100-only; read it if present. */
    char abbrev_name[65];
    char file_type[5] = {0};
    mon_read_sintran_string(ctx, 0, abbrev_name, sizeof(abbrev_name));
    if (ctx->arg_count >= 3) {
        mon_read_sintran_string(ctx, 2, file_type, sizeof(file_type));
    }

    uint32_t output_addr = ctx->arg_addresses[1];

    mon_log(MON_LOG_DEBUG, MON_ID_256B ": IN: AbbrevName='%s', FileType='%s'",
            abbrev_name, file_type[0] ? file_type : "(none)");

    /* Split the abbreviated name into user/name/type so we can resolve it to a
     * host file and check existence. If the name carries no type, fall back to
     * the (ND-100) default file type when one was supplied. */
    char user[SINTRAN_MAX_USER];
    char name[SINTRAN_MAX_NAME];
    char ext[SINTRAN_MAX_TYPE];
    if (mon_parse_sintran_name(abbrev_name, user, sizeof(user),
                               name, sizeof(name), ext, sizeof(ext)) != 0) {
        mon_log(MON_LOG_DEBUG, MON_ID_256B ": unparseable name '%s' -> NO SUCH FILE NAME",
                abbrev_name);
        mon_set_error(ctx, MON_ERR_NO_SUCH_FILE_NAME);  /* 056B (46 dec) */
        return MON_ERROR;
    }
    const char* use_type = ext[0] ? ext : (file_type[0] ? file_type : NULL);

    /* Resolve to a host path and check whether the file exists. Rebuild the
     * (USER)NAME form for the translator so its user handling applies. */
    char sintran_for_xlate[96];
    if (user[0]) {
        snprintf(sintran_for_xlate, sizeof(sintran_for_xlate), "(%s)%s", user, name);
    } else {
        snprintf(sintran_for_xlate, sizeof(sintran_for_xlate), "%s", name);
    }
    char host_path[SINTRAN_MAX_PATH];
    int exists = 0;
    if (mon_translate_path(sintran_for_xlate, use_type, host_path, sizeof(host_path)) == 0) {
        exists = (access(host_path, F_OK) == 0);
    }

    if (!exists) {
        /* Unresolved name: this is the code NC keys on to create the file. */
        mon_log(MON_LOG_DEBUG, MON_ID_256B ": '%s' (host '%s') not found -> NO SUCH FILE NAME",
                abbrev_name, host_path);
        mon_set_error(ctx, MON_ERR_NO_SUCH_FILE_NAME);  /* 056B (46 dec) */
        return MON_ERROR;
    }

    /* Found: build the expanded name. Preserve the caller's directory/name and
     * append the resolved type, then apostrophe-terminate. */
    char full_name[128];
    if (use_type) {
        if (user[0]) {
            snprintf(full_name, sizeof(full_name), "(%s)%s:%s", user, name, use_type);
        } else {
            snprintf(full_name, sizeof(full_name), "%s:%s", name, use_type);
        }
    } else {
        if (user[0]) {
            snprintf(full_name, sizeof(full_name), "(%s)%s", user, name);
        } else {
            snprintf(full_name, sizeof(full_name), "%s", name);
        }
    }

    /* Write full name to output with SINTRAN string terminator (0x27). */
    size_t len = strlen(full_name);
    for (size_t i = 0; i < len; i++) {
        ctx->write_byte(ctx->cpu, output_addr + (uint32_t)i, (uint8_t)full_name[i]);
    }
    ctx->write_byte(ctx->cpu, output_addr + (uint32_t)len, 0x27);

    mon_log(MON_LOG_DEBUG, MON_ID_256B ": OUT: FullName='%s'", full_name);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
