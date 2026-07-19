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

    /* Read the abbreviated name. On ND-500 a STR argument is a [length:4][ptr:4]
     * descriptor, which is how the ND linker passes it: for `OPEN-DOMAIN "A-TEST"`
     * arg 0 is {len=64, ptr-> "A-TEST:DOM'"}. Reading it INLINE (mon_read_sintran_
     * string) read the descriptor's own first byte - the high byte of the length,
     * 0x00 - as a terminator and yielded an EMPTY name, so DEABF returned "no such
     * file" and the linker aborted OPEN-DOMAIN with error 41B. Read the descriptor
     * form first; fall back to inline only if the descriptor is not valid, so a
     * caller that passes an inline string still works. */
    char abbrev_name[65];
    char file_type[5] = {0};
    if (mon_read_descriptor_string(ctx, 0, abbrev_name, sizeof(abbrev_name)) < 0) {
        mon_read_sintran_string(ctx, 0, abbrev_name, sizeof(abbrev_name));
    }
    if (ctx->arg_count >= 3) {
        if (mon_read_descriptor_string(ctx, 2, file_type, sizeof(file_type)) < 0) {
            mon_read_sintran_string(ctx, 2, file_type, sizeof(file_type));
        }
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
    /* Resolve for lookup: own directory first, then the SINTRAN (SYSTEM)
     * fallback for an unqualified name (verified GFILI behaviour, carve
     * 006-S3FS). mon_translate_path_lookup leaves host_path on the own-directory
     * path when the file is found in neither, so the not-found (create) path is
     * unchanged. */
    if (mon_translate_path_lookup(sintran_for_xlate, use_type, host_path, sizeof(host_path)) == 0) {
        exists = (access(host_path, F_OK) == 0);
    }

    if (!exists) {
        /* Unresolved name: this is the code NC keys on to create the file. */
        mon_log(MON_LOG_DEBUG, MON_ID_256B ": '%s' (host '%s') not found -> NO SUCH FILE NAME",
                abbrev_name, host_path);
        mon_set_error(ctx, MON_ERR_NO_SUCH_FILE_NAME);  /* 056B (46 dec) */
        return MON_ERROR;
    }

    /* Found: build the canonical expanded name (DIR:USER)NAME:TYPE;VERSION.
     *
     * DEABF returns "the directory, the user, the file name, the file type, AND the
     * VERSION" (Monitor Calls ND-860228, 256B FULLFILENAME). The version is NOT
     * cosmetic: on a name with no explicit ;version, SINTRAN's GFILI resolver walks
     * the file's on-disk version chain and selects the default (highest) version, and
     * the canonical name it hands back carries that ;VERSION field. Byte-verified in
     * the S3FS carve: FLPAR/MDEAB resolver + GFILI @057173 version chain
     * (057276 "no version requested -> pick default", framing NAME:TYPE;VERSION) --
     *   /mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/L-VSX-500/re/
     *     segments-ref/006-S3FS/{CARVE-ANSWER-FLPAR-MDEAB-FOR-IMPLEMENTER,
     *                            GFILI-COMPLETE-CARVE}.md
     * The ND Linker relies on this: `LOAD B:NRF` resolved to version-less "B:NRF" was
     * rejected with (-677:52) BEFORE any object open; with the ;VERSION it OPENs .NRF.
     *
     * Our host files carry no SINTRAN version chain, so each maps to a single on-disk
     * version whose default is version 1 -- exactly what GFILI's default-version walk
     * yields. So append ";1" UNLESS the resolved name already carries an explicit
     * version (defensive; the linker never sends one). Only FOUND files get a version;
     * a not-yet-created name still returns not-found so NC's create path is unchanged. */
    char base_name[128];
    if (use_type) {
        if (user[0]) snprintf(base_name, sizeof(base_name), "(%s)%s:%s", user, name, use_type);
        else         snprintf(base_name, sizeof(base_name), "%s:%s", name, use_type);
    } else {
        if (user[0]) snprintf(base_name, sizeof(base_name), "(%s)%s", user, name);
        else         snprintf(base_name, sizeof(base_name), "%s", name);
    }
    char full_name[160];
    if (strchr(base_name, ';')) {
        /* Caller already specified a version -> honour it (GFILI explicit-match path). */
        snprintf(full_name, sizeof(full_name), "%s", base_name);
    } else {
        /* No version requested -> GFILI default: highest/only version (1 for host files). */
        snprintf(full_name, sizeof(full_name), "%s;1", base_name);
    }

    /* Write the full name to the OUTPUT argument. On ND-500 arg 1 is also a
     * [length:4][ptr:4] descriptor, so the bytes go to the descriptor's POINTER,
     * not to the descriptor address itself. Writing to the descriptor address
     * corrupted the descriptor; the linker then read its "full name" through a
     * garbage pointer and took a page fault. Terminate with 0x27 (apostrophe).
     * Fall back to writing inline at the arg address if arg 1 is not a valid
     * descriptor (an inline caller). */
    uint32_t out_len = ctx->read_word ? ctx->read_word(ctx->cpu, output_addr) : 0;
    uint32_t out_ptr = ctx->read_word ? ctx->read_word(ctx->cpu, output_addr + 4) : 0;
    uint32_t write_at = (out_len != 0 && out_len <= 10000 && out_ptr != 0)
                        ? out_ptr : output_addr;
    size_t len = strlen(full_name);
    for (size_t i = 0; i < len; i++) {
        ctx->write_byte(ctx->cpu, write_at + (uint32_t)i, (uint8_t)full_name[i]);
    }
    ctx->write_byte(ctx->cpu, write_at + (uint32_t)len, 0x27);

    mon_log(MON_LOG_DEBUG, MON_ID_256B ": OUT: FullName='%s'", full_name);

    /* DEABF must signal success to the linker via K=SET (RETK convention), not
     * K=0. The linker resolves file names with `callg <256B>; if k go <success>`
     * (B004DA08 / B004DA13); a clear K there is read as failure, so LOAD B:NRF
     * aborted with error 52 before ever opening B.NRF even though the name
     * resolved. Per the ND-500 ABI (Ref Manual: monitor calls are domain calls;
     * RET clears K, RETK sets K), a successful domain call leaves K set.
     * mon_set_success clears error_flag/error_code but sets K=0 (our default MON
     * convention), so restore K=1 here for this call's if-k caller. */
    mon_set_success(ctx);
    /* Success return value: W1/i1 = 0. The linker's generic MON trampoline
     * (callg at B004DA08; B004DA11 `w1 =: b.0x44`) copies our returned I1 into
     * the caller's result slot b.0xC, and the wrapper at B004D8DE treats a
     * NONZERO b.0xC as an error code (b.0xC!=0 -> retk with w1=b.0xC). Neither
     * mon_set_success nor the executor writes I1 on success, so without this I1
     * kept the trampoline's leftover call-target value 0xF80000AE
     * (0xF8000000 + 0xAE; 0xAE = 256B octal = DEABF's own routine number). The
     * linker then read 0xF80000AE as an error code from LOAD's name-resolve and
     * re-prompted the object-file field forever instead of opening the file.
     * The DEABF contract is that the caller distinguishes success by W1/i1 == 0
     * (see 256B_FULLFILENAME.yaml return_contract), so set it here. */
    if (ctx->set_i1) {
        ctx->set_i1(ctx->cpu, 0);
    }
    if (ctx->set_k_flag) {
        ctx->set_k_flag(ctx->cpu, 1);
    }
    return MON_SUCCESS;
}
