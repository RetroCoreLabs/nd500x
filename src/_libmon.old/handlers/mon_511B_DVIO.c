/*
 * MON 511B [DVIO/DeviceInputOutput]
 *
 * The FUSED terminal output+input call: it writes a prompt to a device and then
 * reads a line back from it. The carve names its input phase XNINSTR, which is
 * literally MON 503B DVINST's body - so 511B == 504B DVOUTS followed by
 * 503B DVINST on the same device. This handler therefore DELEGATES to the
 * shared DVOUTS/DVINST cores (mon_device_io.h) rather than duplicating them.
 *
 * =========================================================================
 * ARGUMENT MAPPING - ESTABLISHED FROM EVIDENCE (2026-07-16), NOT GUESSED
 * =========================================================================
 *
 * The ND LINKER (/mnt/d/ND/500/nd-linker/linker-b01.dom.asm) has exactly ONE
 * call site for each of 503B/504B/511B, and each is a bare pass-through thunk:
 *
 *   B004AC8A: ents $0x4C
 *   B004AC90: call $0xF8000143,$0xE ,b.0x14,b.0x18,b.0x1C,@b.0x20,b.0x24..b.0x48   ; 503B
 *   B004ACA7: ents $0x20
 *   B004ACAD: call $0xF8000144,$0x3 ,b.0x14,b.0x18,@b.0x1C                         ; 504B
 *   B004ACB9: ents $0x54
 *   B004ACBF: call $0xF8000149,$0x10,b.0x14,b.0x18,@b.0x1C,@b.0x20,b.0x24..b.0x50  ; 511B
 *
 * Nothing runs between `ents` and the `call`, so the thunk's b.0x14.. slots ARE
 * its incoming arguments. Each thunk has ONE caller, which calls it with ZERO
 * CALLG args (`call $0xB004ACB9,$0x0`) after building the block at the top of
 * its own frame (b.0x164..b.0x1A0, i.e. a fixed +0x150 from the thunk's view):
 *
 *   511B caller @ B00474A1..B0047535        503B caller @ B004754F..B00475C0
 *     b.0x164 <- b.0x58        arg0           b.0x164 <- b.0x58        arg0
 *     b.0x168 <- b.0xFC        arg1           b.0x168 <- b.0xF0        arg1
 *     b.0x16C <- laddr @b.0x114+  arg2        b.0x16C  (READ AFTER)    arg2 OUT
 *     b.0x170 <- laddr @b.0xD8+   arg3        b.0x170 <- laddr @b.0xD8+   arg3
 *     b.0x174 <- b.0x100       arg4           b.0x174 <- b.0x100       arg4
 *     b.0x178 <- b.0xF8        arg5           b.0x178 <- b.0xF8        arg5
 *     b.0x17C <- b.0x104       arg6           b.0x17C <- b.0x104       arg6
 *     b.0x180 <- b.0x108       arg7           b.0x180 <- b.0x108       arg7
 *     b.0x184 <- b.0x10C       arg8           b.0x184 <- b.0x10C       arg8
 *     b.0x188 <- b.0x110       arg9           b.0x188 <- b.0x110       arg9
 *     b.0x18C..0x198 <- T[0..3] arg10..13     b.0x18C..0x198 <- T[0..3] arg10..13
 *     b.0x19C <- b.0xF0        arg14          -
 *     b.0x1A0  (READ AFTER)    arg15 OUT      -
 *
 * Three independent confirmations that this mapping is real:
 *   1. 511B args 0..2 are built by an instruction sequence BYTE-IDENTICAL to the
 *      504B DVOUTS caller's (same `w move $0xB0053068,b.0x114` / `by2 laddr
 *      @b.0x114+` / `w2 =: b.0x16C`), and all three callers take arg0 from the
 *      same b.0x58. So args 0..2 ARE DVOUTS' (DeviceNo, NoOfBytes, @Buffer).
 *   2. arg4 is written `w move $0x7,b.0x100` at B004745C; the live probe observed
 *      arg[4] == 7.
 *   3. args 10..13 are read from the table at 0xB0052F04, whose bytes in the
 *      loaded image are FF FF FF FF | 00 00 00 00 | 00 00 00 00 | 00 00 00 03;
 *      the live probe observed arg[10..13] == -1, 0, 0, 3.
 *
 * The arithmetic 3 + (14 - 1 shared DeviceNo) = 16 predicted the COUNT but not
 * the ORDER: DVINST's MaxNo and returned-count are relocated to the END (14, 15)
 * so that indices 1..2 can carry the output phase. A mapping built on that
 * arithmetic would have put MaxNo at arg1 and clobbered the output byte count.
 * This is why it was not guessed.
 *
 *   arg[ 0]  DeviceNo            (1 = own terminal)          } output phase
 *   arg[ 1]  NoOfBytes to write                              } == 504B DVOUTS
 *   arg[ 2]  @output buffer (the prompt)                     }
 *   arg[ 3]  @input buffer                                   } input phase
 *   arg[ 4]  BreakStrat          (observed 7)                } == 503B DVINST
 *   arg[ 5]  EchoStrat           (observed -1)               }    args 3..13,
 *   arg[ 6..9]   break/echo table words or string field      }    same indices
 *   arg[10..13]  table words from 0xB0052F04 {-1,0,0,3}      }
 *   arg[14]  MaxNo to read       (observed 12)
 *   arg[15]  @returned byte count   *** OUT PARAMETER ***
 *
 * arg[15] is written by NOBODY before the call and read immediately after it
 * (`B0047539: w move b.0x1A0,b.0xA8`) - exactly the shape of the MON 412B FSCNT
 * bug. It MUST be written or the caller keeps a stale count.
 *
 * STILL UNPROVEN (do not state as fact): the precise semantics of args 6..9.
 * The caller fills b.0x104.. via an `h smove` from a [len][ptr] descriptor at
 * 0xB0052854 / 0xB005285C, so they are NOT plainly a 128-bit break table in the
 * linker's usage. This handler does not need to resolve that: it passes indices
 * 4..13 through to the SAME DVINST core, which is what the linker's own DVINST
 * call site does with the identical values from the identical frame slots.
 *
 * NOTE: this handler must NOT be registered MON_STATUS_NOT_IMPLEMENTED - the
 * dispatcher (mon_dispatch.c:173) short-circuits that status and never calls the
 * handler at all.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN); carve
 * .../L-VSX-500/re/mon-analysis/511B-DVIO/ (NB: the carve documents 511B at the
 * level-12 driver / ND-500 message-buffer level, NOT the CALLG arg-list model
 * used here - take the arg shape from the call sites above, semantics from the
 * carve).
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_device_io.h"
#include <stdlib.h>

/* Argument indices - see the mapping table above. */
#define DVIO_ARG_DEVICE      0
#define DVIO_ARG_OUT_COUNT   1
#define DVIO_ARG_OUT_BUFFER  2
#define DVIO_ARG_IN_BUFFER   3
#define DVIO_ARG_MAX_BYTES  14
#define DVIO_ARG_RET_COUNT  15

#define DVIO_MIN_ARGS       16

MonResult mon_511B_DVIO(MonContext* ctx) {
    if (ctx->arg_count < DVIO_MIN_ARGS) {
        mon_log(MON_LOG_WARN, MON_ID_511B ": Missing parameters (need %u, got %u)",
                DVIO_MIN_ARGS, ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    if (getenv("ND500X_DVIO_DUMP")) {
        for (uint32_t i = 0; i < ctx->arg_count && i < 20; i++) {
            uint32_t ea = ctx->arg_addresses[i];
            uint32_t v  = mon_read_param_word(ctx, (int)i);
            mon_log(MON_LOG_WARN, "   arg[%u] ea=0x%08X value=0x%08X (%u)", i, ea, v, v);
        }
    }

    uint32_t device_no   = mon_read_param_word(ctx, DVIO_ARG_DEVICE);
    uint32_t out_bytes   = mon_read_param_word(ctx, DVIO_ARG_OUT_COUNT);
    uint32_t out_buffer  = ctx->arg_addresses[DVIO_ARG_OUT_BUFFER];
    uint32_t max_bytes   = mon_read_param_word(ctx, DVIO_ARG_MAX_BYTES);
    uint32_t in_buffer   = ctx->arg_addresses[DVIO_ARG_IN_BUFFER];

    mon_log(MON_LOG_DEBUG, MON_ID_511B
            ": IN: DeviceNo=%u (%o), OutBytes=%u, OutBuf=0x%08X, MaxNo=%u, InBuf=0x%08X",
            device_no, device_no, out_bytes, out_buffer, max_bytes, in_buffer);

    /* ---- Output phase: write the prompt (504B DVOUTS core) ---------------- */
    MonResult r = mon_dvouts_write(ctx, device_no, out_bytes, out_buffer);
    if (r != MON_SUCCESS) {
        mon_log(MON_LOG_WARN, MON_ID_511B ": output phase failed");
        return r;  /* error already set on ctx by the core */
    }

    /* ---- Input phase: read the reply (503B DVINST core) ------------------- *
     * Strategy/table arguments live at indices 4..13 for BOTH calls, so the
     * core reads them from ctx directly. Only DevNo/MaxNo/retcount/buffer move.
     *
     * If the device has no input the core requests a wait: the MON is left
     * UNCOMMITTED, the CPU rewinds to the CALLG and re-dispatches on resume.
     * The prompt is re-written on that retry, which is correct - SINTRAN's DVIO
     * re-prompts when the line is not yet available. */
    r = mon_dvinst_read(ctx, device_no, max_bytes, DVIO_ARG_RET_COUNT, in_buffer);
    if (r != MON_SUCCESS) {
        mon_log(MON_LOG_WARN, MON_ID_511B ": input phase failed");
        return r;  /* error already set on ctx by the core */
    }
    if (ctx->wait_requested) {
        /* Not committed - do not set success; the CALLG will retry. */
        mon_log(MON_LOG_DEBUG, MON_ID_511B ": no input on device %u - suspend and retry",
                device_no);
        return MON_SUCCESS;
    }

    mon_set_success(ctx);
    return MON_SUCCESS;
}
