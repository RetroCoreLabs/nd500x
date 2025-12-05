/*
 * MON 11B (9 decimal): GetBasicTime (TIME)
 *
 * **Time**
 *
 * Gets the current internal time. The internal time is specified in basic time units. There are 50 basic time units in a second.
 *
 * - The internal time is set to 0 each time SINTRAN III is started.
 *
 * Parameters:
 *   [O] BasicTime (LONGINT): output
 *
 * IMPLEMENTATION STATUS: VALIDATED
 */

#include "../mon.h"
#include <time.h>
#include <stdint.h>

/* Emulator start time (set on first call) */
static struct timespec g_start_time = {0, 0};
static int32_t g_start_time_initialized = 0;

MonResult mon_11B_GetBasicTime(MonContext* ctx) {
    /* Initialize start time on first call */
    if (!g_start_time_initialized) {
        clock_gettime(CLOCK_MONOTONIC, &g_start_time);
        g_start_time_initialized = 1;
    }

    /* Get current time */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    /* Calculate elapsed time in basic time units (50 per second) */
    int64_t elapsed_sec = (int64_t)(now.tv_sec - g_start_time.tv_sec);
    int64_t elapsed_nsec = (int64_t)(now.tv_nsec - g_start_time.tv_nsec);
    if (elapsed_nsec < 0) {
        elapsed_sec--;
        elapsed_nsec += 1000000000LL;
    }

    /* Convert to basic time units: 50 units per second = 20ms per unit */
    uint32_t basic_time = (uint32_t)(elapsed_sec * 50 + elapsed_nsec / 20000000);

    /* Write output parameter: BasicTime (LONGINT = 32-bit word) */
    if (ctx->arg_count >= 1) {
        mon_write_param_word(ctx, 0, basic_time);
    }

    mon_log(MON_LOG_DEBUG, "[MON] TIME: returning %u basic time units", basic_time);

    /* Success - no error */
    mon_set_success(ctx);
    return MON_SUCCESS;
}
