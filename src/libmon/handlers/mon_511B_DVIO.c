/*
 * MON 511B [DVIO/DeviceInputOutput]
 *
 * The FUSED terminal output+input call: it writes a prompt to a device and then
 * reads a line back from it. The carve names its input phase XNINSTR, which is
 * literally MON 503B DVINST's body - so 511B == 504B DVOUTS followed by
 * 503B DVINST on the same device.
 *
 * Evidence for the argument shape (the ND LINKER's own call sites,
 * /mnt/d/ND/500/nd-linker/linker-b01.dom.asm):
 *
 *   B004AC90: MON 503B DVINST  $0xE  b.0x14,b.0x18,b.0x1C,@b.0x20,b.0x24,...,b.0x48
 *   B004ACAD: MON 504B DVOUTS  $0x3  b.0x14,b.0x18,@b.0x1C
 *   B004ACBF: MON 511B DVIO    $0x10 b.0x14,b.0x18,@b.0x1C,@b.0x20,b.0x24,...,b.0x50
 *
 * The first three 511B arguments have exactly DVOUTS' shape (DeviceNo,
 * NoOfBytes, @Buffer), and 3 + (14 - 1 shared DeviceNo) = 16 = the observed
 * argument count.
 *
 * OBSERVED ARGUMENTS (real capture, ND LINKER linker-b01.dom, RSIO reporting the
 * terminal as command-input so the linker takes this path):
 *
 *   arg[ 0] = 1            DeviceNo (1 = own terminal)            } confirmed:
 *   arg[ 1] = 2            NoOfBytes to write (the CRLF it emits) } same shape
 *   arg[ 2] ea=0xB0049430  @output buffer                         } as DVOUTS
 *   arg[ 3] ea=0xB0001C2E  input-side buffer / returned count (WHICH? unresolved)
 *   arg[ 4] = 7            arg[ 5] = -1
 *   arg[ 6] = 0x20202020   arg[ 7] = 0x20202020   (ASCII spaces)
 *   arg[ 8] = 0xB0001D48   arg[ 9] = 0xB004D8DE   (pointers)
 *   arg[10] = -1           arg[11] = 0   arg[12] = 0
 *   arg[13] = 3            arg[14] = 12  arg[15] = 0
 *
 * For comparison, 503B DVINST's argument layout in this emulator is:
 *   0 DeviceNo, 1 MaxNo, 2 @returned-count, 3 @buffer, 4 break-strategy,
 *   5 echo-strategy, 6..9 break-table words, 10.. further table words.
 *
 * STATUS: PROBE / IN PROGRESS. The OUTPUT phase (args 0..2) is confirmed. The
 * INPUT phase mapping (args 3..15) is NOT established and must not be guessed:
 * the argument count fits 3 + (14 - 1 shared DeviceNo) = 16, but that arithmetic
 * is not proof. Establish it (e.g. by correlating this dump with the terminal
 * text the linker expects to read) BEFORE wiring the real behaviour, then
 * implement by REUSING the existing 504B DVOUTS + 503B DVINST paths rather than
 * duplicating them.
 *
 * NOTE: this handler must NOT be registered MON_STATUS_NOT_IMPLEMENTED - the
 * dispatcher (mon_dispatch.c) short-circuits that status and never calls the
 * handler at all.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN); carve
 * .../L-VSX-500/re/mon-analysis/511B-DVIO/
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include <stdlib.h>

MonResult mon_511B_DVIO(MonContext* ctx) {
    mon_log(MON_LOG_WARN, MON_ID_511B ": PROBE - called with %u args", ctx->arg_count);

    if (getenv("ND500X_DVIO_DUMP")) {
        for (uint32_t i = 0; i < ctx->arg_count && i < 20; i++) {
            uint32_t ea = ctx->arg_addresses[i];
            uint32_t v  = mon_read_param_word(ctx, (int)i);
            mon_log(MON_LOG_WARN, "   arg[%u] ea=0x%08X value=0x%08X (%u)", i, ea, v, v);
        }
    }

    /* Not yet implemented: report the call and return an error rather than
     * pretending it succeeded, so callers do not silently take a wrong path. */
    mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);
    return MON_ERROR;
}
