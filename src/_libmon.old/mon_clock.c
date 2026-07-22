/*
 * mon_clock - Deterministic clock source for MON time/date calls.
 * See mon_clock.h for the contract.
 */
#include "mon_clock.h"
#include <stdint.h>
#include <stdlib.h>

static int      g_deterministic = 0;
static time_t   g_fixed_epoch = MON_CLOCK_PINNED_EPOCH_DEFAULT;

/* TUSED session baseline (real-clock mode only). */
static int             g_session_initialized = 0;
static struct timespec g_session_start;

static void ensure_session(void) {
    if (!g_session_initialized) {
        clock_gettime(CLOCK_MONOTONIC, &g_session_start);
        g_session_initialized = 1;
    }
}

void mon_clock_set_deterministic(time_t fixed_epoch_utc) {
    g_deterministic = 1;
    g_fixed_epoch = fixed_epoch_utc;
}

void mon_clock_clear_deterministic(void) {
    g_deterministic = 0;
}

bool mon_clock_is_deterministic(void) {
    return g_deterministic != 0;
}

time_t mon_clock_now(void) {
    return g_deterministic ? g_fixed_epoch : time(NULL);
}

struct tm* mon_clock_breakdown(time_t t, struct tm* out) {
    if (!out) return NULL;
    if (g_deterministic) {
        /* UTC, so the fields match the agreed UTC instant regardless of the
         * host timezone. */
#ifdef _WIN32
        return (gmtime_s(out, &t) == 0) ? out : NULL;
#else
        return gmtime_r(&t, out);
#endif
    }
#ifdef _WIN32
    return (localtime_s(out, &t) == 0) ? out : NULL;
#else
    return localtime_r(&t, out);
#endif
}

uint32_t mon_clock_tused_basic_units(void) {
    if (g_deterministic) {
        return 0u;  /* No elapsed CPU time in reproducible runs. */
    }
    ensure_session();
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t elapsed_ns = (int64_t)(now.tv_sec - g_session_start.tv_sec) * 1000000000LL
                       + (int64_t)(now.tv_nsec - g_session_start.tv_nsec);
    int64_t basic_units = elapsed_ns / 20000000LL;  /* 20 ms per unit */
    if (basic_units < 0) basic_units = 0;
    if (basic_units > 0xFFFFFFFFLL) basic_units = 0xFFFFFFFFLL;
    return (uint32_t)basic_units;
}

void mon_clock_reset_session(void) {
    clock_gettime(CLOCK_MONOTONIC, &g_session_start);
    g_session_initialized = 1;
}
