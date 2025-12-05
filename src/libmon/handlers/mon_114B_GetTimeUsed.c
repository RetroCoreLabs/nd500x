/*
 * MON 114B (76 decimal): GetTimeUsed (TUSED)
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
#include <time.h>

/* Session start time - initialized on first call */
static int g_time_initialized = 0;
static struct timespec g_session_start;

/* Initialize session start time */
static void init_session_time(void) {
    if (!g_time_initialized) {
        clock_gettime(CLOCK_MONOTONIC, &g_session_start);
        g_time_initialized = 1;
    }
}

/* Reset session time (can be called from mon_init or debugger) */
void mon_reset_session_time(void) {
    clock_gettime(CLOCK_MONOTONIC, &g_session_start);
    g_time_initialized = 1;
}

MonResult mon_114B_GetTimeUsed(MonContext* ctx) {
    struct timespec now;
    uint32_t time_used;

    /* Ensure session time is initialized */
    init_session_time();

    /* Get current time */
    clock_gettime(CLOCK_MONOTONIC, &now);

    /* Calculate elapsed time in basic time units (1/50s = 20ms)
     *
     * elapsed_seconds = now.tv_sec - start.tv_sec
     * elapsed_nsec = now.tv_nsec - start.tv_nsec
     *
     * basic_units = elapsed_seconds * 50 + elapsed_nsec / 20000000
     *             = (elapsed_seconds * 1000000000 + elapsed_nsec) / 20000000
     */
    int64_t elapsed_ns = (int64_t)(now.tv_sec - g_session_start.tv_sec) * 1000000000LL
                       + (int64_t)(now.tv_nsec - g_session_start.tv_nsec);

    /* Convert to basic time units (20ms = 20,000,000 ns) */
    int64_t basic_units = elapsed_ns / 20000000LL;

    /* Clamp to 32-bit (LONGINT) - about 994 days max */
    if (basic_units > 0xFFFFFFFF) {
        basic_units = 0xFFFFFFFF;
    }

    time_used = (uint32_t)basic_units;

    /* Return result in W1 (I1) register */
    if (ctx->set_i1) {
        ctx->set_i1(ctx->cpu, time_used);
    }

    /* Log the result */
    mon_log(MON_LOG_INFO, "MON 114B TUSED: TimeUsed = %u basic time units (%.2f seconds)",
            time_used, (double)time_used / 50.0);

    /* Set success (K=0) */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
