/*
 * MON 321B [UEADM/UEAdministrator] - User Environment administrator.
 *
 * NOT deprecated. The carve proves this is a live, complete handler in SINTRAN
 * L07: MCTAB[321B] = 065453B = UEADM, a real ~100-word worker in segment
 * 003-S3CP. The manual's "no longer supported" note (section 2.16) does not
 * match the carved bytes, and the ND Linker depends on the call succeeding.
 *
 * WHAT THE WORKER DOES (byte-anchored from
 *   /mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/L-VSX-500/
 *   re/mon-analysis/321B-UEAdministrator/321B-UEAdministrator.ASM
 * and segment 003-S3CP, all addresses octal):
 *
 *   - Reads a sub-function SELECTOR from param[12] and range-checks it to
 *     [1..8] (065461-065467: SAT 1 / SKP IF DA GRE ST ... SAT 10 /
 *     SKP IF DT LST SA). Out of range -> stores error code 124B into the
 *     selector slot and returns via the non-skip (error) path (065470-065472).
 *   - Takes a reservation/critical-section lock via CCBRS (065507 JPL I 111 ->
 *     030130B) - IOF/ION bracket, MST PID, MIN - purely to serialise access.
 *   - Reads a per-user record (stride 074B = 60 words) from a resident table
 *     whose base and mapping descriptor sit at absolute low memory 004321B /
 *     004320B (065527 MPY 74 / ADD I 74 / LDT I 73 / LDATX). NO file I/O: the
 *     worker body 065453B-065747B contains no MON/OPEN/RFILE opcode, and the
 *     User-Environment subsystem reads no UE-ERMSG / message-text file here.
 *   - Dispatches an 8-arm computed jump (065600 LDA ,B -170 / RADD SA DP, arms
 *     at 065602B + selector). For SELECTOR 1 the arm at 065603B (JMP I 32 ->
 *     ptr@065635 = 066455B) runs the routine at 066455B, which funnels to the
 *     shared success epilogue at 066644B. That epilogue's only success/error
 *     distinction is `066644 MIN ,B -167`, a SKIP-RETURN => the MON returns
 *     with the ERROR (K) flag CLEAR. The error branch instead writes a code
 *     into the selector slot and takes the non-skip return.
 *
 * WHY THE OLD STUB CRASHED THE LINKER: it returned K=1 with error 124 for
 * every call, including the in-range selector 1. The linker's UEADM wrapper at
 * B004D383 does `if-kgo $6`, branching straight to its `ret` WITHOUT reloading
 * w1 on the error path, so the injected 124 travelled on as a bogus result and
 * eventually produced a null string descriptor that the linker's own unguarded
 * `by comp2 IND(b.20)(r1),W2` at B0036975 dereferenced -> trap 7644B PROTECT
 * VIOLATION. HELP and every abbreviated command (e.g. REFER) hit this.
 *
 * ND-500 CALL SHAPE (observed live): 3 by-reference args. arg[0] = the
 * selector (=1 in every observed call), arg[1] and arg[2] operands. The full
 * ND-100 param block ([10],[17],[32],[44]...) has no ND-500 counterpart here;
 * the linker uses a reduced 3-word contract that only exercises selector 1.
 *
 * EMULATION CONTRACT (byte-proven parts marked):
 *   - selector in [1..8] -> SUCCESS, K flag CLEAR.            [byte-proven]
 *   - selector out of range -> error 124B written into the
 *     selector slot (arg[0]), K flag SET.                    [byte-proven]
 *   - w1 on success: the ND-100 worker leaves A = the param-block base and
 *     conveys its real result via the skip-return, not via A. The linker's
 *     wrapper pre-zeroes its result slot, so we return w1 = 0 ("no error").
 *                                                              [inference]
 *
 * NOT YET SUFFICIENT FOR THE HELP/REFER CRASH. Returning K clear is NECESSARY
 * but, tested live, does NOT stop the trap at B0036975: with all UEADM calls
 * succeeding (HELP issues three - selectors 1,1,2), the linker still walks a
 * null string descriptor and aborts. The remaining cause is that UEADM must
 * ALSO write per-user data into a caller buffer that the linker later walks;
 * the CONTENT of that buffer is not byte-proven by the carve (the selector-1
 * arm at 066455B does file housekeeping and a skip-return, not an obvious
 * param-block payload write). We do NOT invent that payload here. So this
 * handler is the byte-proven DISPOSITION only; the crash stays open pending a
 * carve of what UEADM writes back, or a live trace of the buffer the linker
 * walks. See /home/ronny/repos/nd500x/docs/CARVE-QUESTION-321B-UEADM.md.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN); carve L-VSX-500.
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"

MonResult mon_321B_UEAdministrator(MonContext* ctx) {
    /* Sub-function selector = param[12] on ND-100, arg[0] on ND-500. The linker
     * always passes 1. If the caller supplied no argument at all, treat it as
     * the in-range default rather than faulting. */
    uint32_t selector = (ctx->arg_count >= 1) ? mon_read_param_word(ctx, 0) : 1;

    mon_log(MON_LOG_DEBUG, MON_ID_321B ": IN: selector=%u (args=%u)",
            selector, ctx->arg_count);

    /* Range-check [1..8], exactly as the worker does at 065461-065467. */
    if (selector < 1 || selector > 8) {
        mon_log(MON_LOG_WARN, MON_ID_321B
                ": selector %u out of range [1..8] - returning error 124B",
                selector);
        /* The worker stores 124B into the selector slot on the error path
         * (065470 LDA 124 / 065471 STA ,X 12). */
        if (ctx->arg_count >= 1) {
            mon_write_param_word(ctx, 0, MON_ERR_ILLEGAL_PARAMETER);
        }
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B, K flag set */
        return MON_ERROR;
    }

    /* In range -> the worker's success epilogue at 066644B takes the skip
     * return (K clear). We expose no per-user table; return w1 = 0. */
    if (ctx->set_i1) {
        ctx->set_i1(ctx->cpu, 0);
    }
    mon_log(MON_LOG_INFO, MON_ID_321B ": OUT: selector %u handled (success)",
            selector);
    mon_set_success(ctx);
    return MON_SUCCESS;
}
