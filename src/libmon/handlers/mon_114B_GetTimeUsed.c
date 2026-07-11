/*
 * MON 114B [TUSED/GetTimeUsed]
 *
 * Gets the time you have used the CPU since you logged in. In batch jobs,
 * you get the time since you entered the job.
 *
 * - The CPU time used is given in basic time units (1/50th of a second = 20ms).
 * - Can also be used from RT-programs.
 *
 * Parameters:
 *   [O] TimeUsed (LONGINT): CPU time used in basic time units. Output in W1 (I1).
 *
 * Reference: ND-860228.2 EN (SINTRAN III Monitor Calls)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_clock.h"

/* Reset session time (can be called from mon_init or debugger).
 * The TUSED baseline now lives in mon_clock; kept as a thin alias so existing
 * callers continue to work. */
void mon_reset_session_time(void) {
    mon_clock_reset_session();
}

MonResult mon_114B_GetTimeUsed(MonContext* ctx) {
    /* Elapsed CPU time in basic time units. In deterministic (pinned-clock)
     * mode this returns 0 so the run is bit-reproducible; otherwise it is the
     * real host CLOCK_MONOTONIC elapsed since session start. */
    uint32_t time_used = mon_clock_tused_basic_units();

    /* Return result in W1 (I1) register */
    if (ctx->set_i1) {
        ctx->set_i1(ctx->cpu, time_used);
    }

    /* Log the result */
    mon_log(MON_LOG_INFO, MON_ID_114B ": IN: (none)");
    mon_log(MON_LOG_INFO, MON_ID_114B ": OUT: TimeUsed=%o (%.2f seconds)",
            time_used, (double)time_used / 50.0);

    /* Set success (K=0) */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
