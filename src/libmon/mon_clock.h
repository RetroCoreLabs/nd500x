/*
 * mon_clock - Deterministic clock source for MON time/date calls
 *
 * By default (real-machine behaviour) all MON time/date calls report the real
 * host clock. For reproducible runs (cross-emulator PC-diff, regression traces)
 * the clock can be pinned to a fixed UTC instant. When pinned:
 *   - MON 113B CLOCK / 142B error timestamps return the fixed instant (UTC).
 *   - MON 114B TUSED returns 0 basic time units (no elapsed CPU time).
 *   - 41B ROBJE object-entry file dates return the fixed instant.
 *
 * The pinned instant is shared with the RetroCore (C#) emulator so both produce
 * an identical MON time stream; the agreed value is 1990-01-01 12:00:00 UTC
 * (Unix epoch 631195200). See docs/NC_CRASH_0x08023EA4_ROOTCAUSE.md.
 */
#ifndef MON_CLOCK_H
#define MON_CLOCK_H

#include <time.h>
#include <stdbool.h>
#include <stdint.h>

/* Agreed cross-emulator fixed instant: 1990-01-01 12:00:00 UTC. */
#define MON_CLOCK_PINNED_EPOCH_DEFAULT ((time_t)631195200)

/* Pin the clock to a fixed UTC instant (deterministic mode ON). */
void mon_clock_set_deterministic(time_t fixed_epoch_utc);

/* Return to real host clock (deterministic mode OFF). */
void mon_clock_clear_deterministic(void);

/* True when the clock is pinned. */
bool mon_clock_is_deterministic(void);

/* Current time_t: the pinned instant when deterministic, else time(NULL). */
time_t mon_clock_now(void);

/* Break a time_t into calendar fields. Deterministic mode uses UTC (gmtime)
 * so the result matches the agreed UTC instant exactly; otherwise localtime.
 * Returns out on success, NULL on failure. */
struct tm* mon_clock_breakdown(time_t t, struct tm* out);

/* MON 114B TUSED value in basic time units (1/50 s). Returns 0 when pinned. */
uint32_t mon_clock_tused_basic_units(void);

/* Reset the TUSED session baseline to "now" (real-clock mode only). */
void mon_clock_reset_session(void);

#endif /* MON_CLOCK_H */
