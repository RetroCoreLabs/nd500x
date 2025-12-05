/*
 * SINTRAN III Monitor Call Logging
 *
 * Provides configurable logging for MON call entry/exit,
 * input parameters, and output results.
 */

#ifndef MON_LOG_H
#define MON_LOG_H

#include "mon_types.h"
#include <stdarg.h>

/* =========================================================================
 * LOG LEVELS
 * ========================================================================= */

typedef enum {
    MON_LOG_OFF = 0,        /* No logging */
    MON_LOG_ERROR = 1,      /* Errors only (unimplemented MON, failures) */
    MON_LOG_WARN = 2,       /* Warnings (in-progress MON called) */
    MON_LOG_INFO = 3,       /* Basic MON call entry/exit */
    MON_LOG_DEBUG = 4,      /* Parameters and return values */
    MON_LOG_TRACE = 5       /* Detailed data (hex dumps, string contents) */
} MonLogLevel;

/* =========================================================================
 * LOG OUTPUT CALLBACK
 *
 * User-provided function to receive log messages.
 * If not set, logs go to stderr.
 * ========================================================================= */

typedef void (*MonLogCallback)(MonLogLevel level, const char* message);

/* =========================================================================
 * LOGGING CONFIGURATION API
 * ========================================================================= */

/* Enable/disable logging globally (master switch) */
void mon_log_enable(int enabled);
int mon_log_is_enabled(void);

/* Set logging level */
void mon_log_set_level(MonLogLevel level);
MonLogLevel mon_log_get_level(void);

/* Set custom log output callback */
void mon_log_set_callback(MonLogCallback callback);

/* =========================================================================
 * LOGGING FUNCTIONS (for handlers and dispatcher)
 * ========================================================================= */

/* General logging with level and printf-style format */
void mon_log(MonLogLevel level, const char* fmt, ...);
void mon_logv(MonLogLevel level, const char* fmt, va_list args);

/* Log MON call entry with input parameters */
void mon_log_entry(MonContext* ctx, const char* name);

/* Log MON call exit with result and output parameters */
void mon_log_exit(MonContext* ctx, const char* name, MonResult result);

/* =========================================================================
 * PARAMETER LOGGING HELPERS
 *
 * Call these from handlers to log specific parameters.
 * Only outputs if current log level >= MON_LOG_DEBUG.
 * ========================================================================= */

/* Log a word (32-bit) parameter */
void mon_log_param_word(MonContext* ctx, int idx, const char* name, MonParamIO io);

/* Log a double-word (64-bit) parameter */
void mon_log_param_dword(MonContext* ctx, int idx, const char* name, MonParamIO io);

/* Log a byte parameter */
void mon_log_param_byte(MonContext* ctx, int idx, const char* name, MonParamIO io);

/* Log a string parameter (reads from string descriptor) */
void mon_log_param_string(MonContext* ctx, int idx, const char* name, MonParamIO io);

/* Log a buffer/memory block (hex dump) */
void mon_log_param_buffer(MonContext* ctx, uint32_t addr, int len, const char* name, MonParamIO io);

/* =========================================================================
 * UTILITY MACROS FOR HANDLERS
 *
 * Example usage in handler:
 *   MON_LOG_IN_WORD(ctx, 0, "Device");
 *   MON_LOG_IN_BYTE(ctx, 1, "Byte");
 *   ... do work ...
 *   MON_LOG_OUT_WORD(ctx, 2, "BytesWritten");
 * ========================================================================= */

#define MON_LOG_IN_WORD(ctx, idx, name)   mon_log_param_word((ctx), (idx), (name), MON_PARAM_IN)
#define MON_LOG_IN_BYTE(ctx, idx, name)   mon_log_param_byte((ctx), (idx), (name), MON_PARAM_IN)
#define MON_LOG_IN_DWORD(ctx, idx, name)  mon_log_param_dword((ctx), (idx), (name), MON_PARAM_IN)
#define MON_LOG_IN_STRING(ctx, idx, name) mon_log_param_string((ctx), (idx), (name), MON_PARAM_IN)

#define MON_LOG_OUT_WORD(ctx, idx, name)   mon_log_param_word((ctx), (idx), (name), MON_PARAM_OUT)
#define MON_LOG_OUT_BYTE(ctx, idx, name)   mon_log_param_byte((ctx), (idx), (name), MON_PARAM_OUT)
#define MON_LOG_OUT_DWORD(ctx, idx, name)  mon_log_param_dword((ctx), (idx), (name), MON_PARAM_OUT)
#define MON_LOG_OUT_STRING(ctx, idx, name) mon_log_param_string((ctx), (idx), (name), MON_PARAM_OUT)

#endif /* MON_LOG_H */
