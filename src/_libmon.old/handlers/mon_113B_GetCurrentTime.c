/*
 * MON 113B [CLOCK/GetCurrentTime]
 *
 * Gets the current system time and date.
 *
 * - The current system time is returned as basic time units, seconds, minutes,
 *   hours, day, month, and year.
 * - Basic time units = 50 per second (20ms each).
 *
 * Parameters:
 *   [O] TimeBuffer (ARRAY of 7 INTEGERs):
 *       [0] Basic time units since midnight
 *       [1] Seconds (0-59)
 *       [2] Minutes (0-59)
 *       [3] Hours (0-23)
 *       [4] Day (1-31)
 *       [5] Month (1-12)
 *       [6] Year (last two digits, e.g., 85 for 1985)
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_clock.h"

/* Basic time units per second */
#define BASIC_TIME_UNITS_PER_SEC 50

MonResult mon_113B_GetCurrentTime(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, MON_ID_113B ": Missing parameters (need 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Get output buffer address */
    uint32_t buffer_addr = ctx->arg_addresses[0];

    /* Get current time via the MON clock (pinned UTC instant in deterministic
     * mode, else real host localtime). */
    time_t now = mon_clock_now();
    struct tm tm_storage;
    struct tm* tm_now = mon_clock_breakdown(now, &tm_storage);

    if (!tm_now) {
        mon_log(MON_LOG_WARN, MON_ID_113B ": Failed to get local time");
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    /* Calculate basic time units since midnight
     * = hours * 3600 * 50 + minutes * 60 * 50 + seconds * 50
     */
    uint32_t basic_units = (uint32_t)(tm_now->tm_hour * 3600 + tm_now->tm_min * 60 + tm_now->tm_sec)
                         * BASIC_TIME_UNITS_PER_SEC;

    /* Write 7 words to buffer */
    /* [0] Basic time units since midnight */
    ctx->write_word(ctx->cpu, buffer_addr + 0, basic_units);

    /* [1] Seconds (0-59) */
    ctx->write_word(ctx->cpu, buffer_addr + 4, (uint32_t)tm_now->tm_sec);

    /* [2] Minutes (0-59) */
    ctx->write_word(ctx->cpu, buffer_addr + 8, (uint32_t)tm_now->tm_min);

    /* [3] Hours (0-23) */
    ctx->write_word(ctx->cpu, buffer_addr + 12, (uint32_t)tm_now->tm_hour);

    /* [4] Day (1-31) */
    ctx->write_word(ctx->cpu, buffer_addr + 16, (uint32_t)tm_now->tm_mday);

    /* [5] Month (1-12) - tm_mon is 0-11 */
    ctx->write_word(ctx->cpu, buffer_addr + 20, (uint32_t)(tm_now->tm_mon + 1));

    /* [6] Year (last two digits) - tm_year is years since 1900 */
    ctx->write_word(ctx->cpu, buffer_addr + 24, (uint32_t)(tm_now->tm_year % 100));

    mon_log(MON_LOG_DEBUG, MON_ID_113B ": IN: (none)");
    mon_log(MON_LOG_DEBUG, MON_ID_113B ": OUT: Time=%02d:%02d:%02d Date=%02d/%02d/%02d BasicUnits=%o",
            tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
            tm_now->tm_mday, tm_now->tm_mon + 1, (tm_now->tm_year % 100),
            basic_units);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
