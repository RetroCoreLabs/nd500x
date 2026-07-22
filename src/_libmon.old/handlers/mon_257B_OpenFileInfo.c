/*
 * MON 257B (175 decimal): OpenFileInfo (FOPEN)
 *
 * Gets information about an ALREADY-OPEN file. The caller specifies the file
 * name (+ type); the call returns the open file number and the access code.
 * For peripheral files it returns the logical device number.
 *
 * The ND Linker (B01) uses this on the "MON 257B present" code path (SINTRAN L
 * has it: MCTAB[257B]=0111212B, non-zero, carve-verified in segment
 * 044-S3IDPIT of L-VSX-500) to find the file number of its already-open
 * error-message file (UE-ERMSG-EN-C:ERR) before reading a message block from
 * it, instead of re-opening the file on every message.
 *
 * ND-500 CALLG form as issued by the linker takes 5 args:
 *   [I] FileName   (STRING descriptor [len:4][ptr:4])
 *   [I] FileType   (STRING descriptor [len:4][ptr:4])
 *   [O] FileNo     (INTEGER word)  - open file number (64..127) if found
 *   [O] AccessCode (INTEGER word)  - 0=read, 1=write, 2=read+write
 *   [O] DevNo      (INTEGER word)  - LDN for peripheral files, else 0
 * On "file not open" the call returns error (K clear) with a SINTRAN error code
 * in W1, matching how the linker distinguishes open from not-open.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN), 257B FOPEN.
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Read an ND-500 STRING argument that is a [len:4][ptr:4] descriptor. Copies
 * up to max-1 bytes of the target text into out (NUL-terminated), stripping a
 * trailing 0x27 apostrophe if present. Returns the logical length. */
static uint32_t read_desc_string(MonContext* ctx, int arg, char* out, size_t max) {
    uint32_t ea = ctx->arg_addresses[arg];
    uint32_t len = ctx->read_word(ctx->cpu, ea);
    uint32_t ptr = ctx->read_word(ctx->cpu, ea + 4);
    uint32_t n = 0;
    for (uint32_t i = 0; i < len && n < max - 1; i++) {
        uint8_t b = ctx->read_byte(ctx->cpu, ptr + i);
        if (b == 0x27 || b == 0) break;
        out[n++] = (char)b;
    }
    out[n] = 0;
    return n;
}

/* Compare a table object_name (may be 0x27- or NUL-terminated, fixed field)
 * against a C string, case-insensitively. */
static int name_eq(const char* field, size_t field_max, const char* want) {
    size_t i = 0;
    for (; i < field_max; i++) {
        char c = field[i];
        if (c == 0x27 || c == 0) break;
        if (!want[i]) return 0;
        if (tolower((unsigned char)c) != tolower((unsigned char)want[i])) return 0;
    }
    return want[i] == 0;
}

MonResult mon_257B_OpenFileInfo(MonContext* ctx) {
    if (ctx->arg_count < 3) {
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);
        return MON_ERROR;
    }

    char want_name[64];
    char want_type[16];
    read_desc_string(ctx, 0, want_name, sizeof(want_name));
    read_desc_string(ctx, 1, want_type, sizeof(want_type));

    mon_log(MON_LOG_DEBUG, MON_ID_257B ": IN: FileName='%s', FileType='%s'",
            want_name, want_type);

    /* Search the open-file table for a matching name (+ type when supplied). */
    int found_no = -1;
    OpenFileEntry* found = NULL;
    for (int fn = FILE_NUMBER_MIN; fn <= FILE_NUMBER_MAX; fn++) {
        OpenFileEntry* e = mon_file_table_get(fn);
        if (!e || !e->in_use) continue;
        if (!name_eq(e->object_entry.object_name, sizeof(e->object_entry.object_name), want_name))
            continue;
        if (want_type[0] &&
            !name_eq(e->object_entry.type, sizeof(e->object_entry.type), want_type))
            continue;
        found_no = fn;
        found = e;
        break;
    }

    if (found_no < 0) {
        /* Not open. Return the SINTRAN "no such open file" style error via K clear
         * so the linker's if-not-k branch opens the file itself (fallback path). */
        mon_log(MON_LOG_DEBUG, MON_ID_257B ": '%s:%s' is not open -> not found",
                want_name, want_type[0] ? want_type : "(any)");
        mon_set_error(ctx, MON_ERR_NO_SUCH_FILE_NAME); /* 056B */
        return MON_ERROR;
    }

    /* Map access_mode (SINTRAN open code) to FOPEN AccessCode {0 read,1 write,
     * 2 read+write}. */
    uint32_t acc;
    switch (found->access_mode) {
        case ACCESS_SEQ_WRITE:
        case ACCESS_SEQ_APPEND:
            acc = 1; break;                 /* write */
        case ACCESS_SEQ_READ:
        case ACCESS_RAND_READ:
        case ACCESS_RAND_READ_CTG:
            acc = 0; break;                 /* read */
        default:
            acc = 2; break;                 /* read+write */
    }
    uint32_t devno = found->object_entry.device_number;

    if (ctx->arg_count >= 3) mon_write_param_word(ctx, 2, (uint32_t)found_no);
    if (ctx->arg_count >= 4) mon_write_param_word(ctx, 3, acc);
    if (ctx->arg_count >= 5) mon_write_param_word(ctx, 4, devno);

    mon_log(MON_LOG_DEBUG, MON_ID_257B ": OUT: FileNo=%d (0%oB) AccessCode=%u DevNo=%u",
            found_no, found_no, acc, devno);

    /* Found -> success, K set (open-file query succeeded). */
    mon_set_success(ctx);
    if (ctx->set_k_flag) ctx->set_k_flag(ctx->cpu, 1);
    return MON_SUCCESS;
}
