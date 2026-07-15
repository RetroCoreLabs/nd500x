/*
 * MON 162B [OUTST/OutString]
 *
 * Writes a string of characters to a peripheral file, e.g., a terminal or a printer.
 *
 * - You cannot use this monitor call for mass-storage files.
 * - The output buffer of the device may be too small. Then the program waits
 *   until the required buffer space becomes available.
 * - Parameters are fetched and returned through the alternative page table.
 * - The maximum string length is 2048 bytes (as in OutputString).
 * - For performance reasons, it is inadvisable to use this call from the ND-500.
 *   Use OutputString (504B) instead.
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): Logical device number. Cannot use 1 for own terminal.
 *       Use ExecutionInfo to get its logical device number. File numbers are illegal.
 *   [I] TextWrite (STRING): Address of character string to be output (max 2048 bytes).
 *   [I] NoOfBytes (INTEGER): Number of characters to write.
 *   [O] ReturnStatus (INTEGER): Return status (output).
 *
 * Returns:
 *   K flag set on error, error code in I1
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include <stdlib.h>
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"
#include <stdio.h>

#define MAX_OUTSTRING_LENGTH 2048

/* Output-string control bytes (carve 162B-OutString OUTST loop, byte-verified):
 *   047B (0x27, apostrophe ') = STRING TERMINATOR - stop output here.
 *   044B (0x24, dollar '$')   = emit '$' then a line feed (012B).
 * The ND LINKER calls OUTST in a 2-ARG, terminator-driven form (DeviceNo, Text)
 * with NO byte count; our old code required a 3-arg count form and errored,
 * so the linker's banner/prompt never appeared. Accept 2 or 3+ args: an
 * explicit NoOfBytes (arg2) caps the length, and the 0x27 terminator always
 * stops output. */
#define OUTST_TERMINATOR 0x27u
#define OUTST_DOLLAR     0x24u

MonResult mon_162B_OutString(MonContext* ctx) {
    /* The linker uses the 2-arg form; older callers pass 3 (with NoOfBytes). */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_162B ": Missing parameters (need >=2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t text_addr = ctx->arg_addresses[1];

    /* NoOfBytes is optional (3-arg form only); the 0x27 terminator bounds the
     * 2-arg form. Always cap at MAX_OUTSTRING_LENGTH for safety. */
    uint32_t limit = MAX_OUTSTRING_LENGTH;
    if (ctx->arg_count >= 3) {
        uint32_t n = mon_read_param_word(ctx, 2);
        MON_LOG_IN_WORD(ctx, 2, "NoOfBytes");
        if (n < limit) limit = n;
    }

    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    mon_log(MON_LOG_DEBUG, MON_ID_162B ": IN: DeviceNo=%o, TextAddr=0x%08X, args=%u, limit=%u",
            device_no, text_addr, ctx->arg_count, limit);

    /* TEMP Phase-2 probe: is arg1 a raw string or a descriptor {count,base}? */
    if (getenv("ND500X_OUTST_DUMP")) {
        uint32_t w0 = 0, w1 = 0;
        for (int k = 0; k < 4; k++) { w0 = (w0<<8)|ctx->read_byte(ctx->cpu, text_addr+k); }
        for (int k = 0; k < 4; k++) { w1 = (w1<<8)|ctx->read_byte(ctx->cpu, text_addr+4+k); }
        char raw[17], viab[17];
        for (int k = 0; k < 16; k++) { uint8_t c=ctx->read_byte(ctx->cpu,text_addr+k); raw[k]=(c>=32&&c<127)?(char)c:'.'; }
        raw[16]=0;
        for (int k = 0; k < 16; k++) { uint8_t c=ctx->read_byte(ctx->cpu,w1+k); viab[k]=(c>=32&&c<127)?(char)c:'.'; }
        viab[16]=0;
        mon_log(MON_LOG_WARN, "  OUTST arg1=0x%08X: word0=0x%08X word1=0x%08X raw='%s' via_base(0x%08X)='%s'",
                text_addr, w0, w1, raw, w1, viab);
    }

    /* File numbers are illegal for this call; other devices route to the
     * console (this is a peripheral/terminal output call). */
    if (is_mass_storage_file(device_no)) {
        mon_log(MON_LOG_WARN, MON_ID_162B ": File numbers not allowed (device %o)", device_no);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        if (ctx->arg_count >= 4) mon_write_param_word(ctx, 3, 52);
        return MON_ERROR;
    }

    /* String-source resolution:
     *  - 2-arg form (the LINKER): arg1 is the address of an ND-500 STRING
     *    DESCRIPTOR {element_count@+0, base_address@+4} (big-endian words), the
     *    same layout SCOMP/SCOPA use. Output element_count bytes from base.
     *  - 3+-arg form: arg1 is the raw character address, bounded by NoOfBytes.
     * Both still honour the 0x27 terminator. (Verified: the linker's arg1
     * 0xB0056B34 = {count=0xC8, base=0xB0056A6C}; the raw bytes there are the
     * descriptor, not text.) */
    uint32_t src_addr = text_addr;
    if (ctx->arg_count == 2) {
        uint32_t count = 0, base = 0;
        for (int k = 0; k < 4; k++) count = (count << 8) | ctx->read_byte(ctx->cpu, text_addr + k);
        for (int k = 0; k < 4; k++) base  = (base  << 8) | ctx->read_byte(ctx->cpu, text_addr + 4 + k);
        src_addr = base;
        if (count < limit) limit = count;
        mon_log(MON_LOG_DEBUG, MON_ID_162B ": descriptor count=%u base=0x%08X", count, base);
    }

    ConsoleIO* console = mon_file_table_get_console();
    uint32_t written = 0;
    for (uint32_t i = 0; i < limit; i++) {
        uint8_t b = ctx->read_byte(ctx->cpu, src_addr + i);
        if (b == OUTST_TERMINATOR) {
            break;  /* apostrophe terminates the string */
        }
        if (b == OUTST_DOLLAR) {
            /* '$' emits itself followed by a line feed (carve 41043-41052). */
            if (console && console->write_char) {
                console->write_char(console->context, (char)OUTST_DOLLAR);
                console->write_char(console->context, '\n');
            } else { putchar('$'); putchar('\n'); }
            written += 2;
            continue;
        }
        if (console && console->write_char) {
            console->write_char(console->context, (char)b);
        } else {
            putchar(b);
        }
        written++;
    }

    if (!console || !console->write_char) fflush(stdout);

    /* Status output slot (only present in the extended arg form). */
    if (ctx->arg_count >= 4) mon_write_param_word(ctx, 3, 0);  /* 0 = success */

    mon_log(MON_LOG_DEBUG, MON_ID_162B ": OUT: Wrote %u bytes to device %o", written, device_no);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
