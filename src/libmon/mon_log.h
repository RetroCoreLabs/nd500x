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
 * MON CALL IDENTIFIER MACROS
 *
 * Use these in mon_log() calls for consistent formatting:
 *   mon_log(MON_LOG_DEBUG, MON_ID_11B ": OUT: BasicTime=%u", value);
 *
 * Format: "MON nnnB [ShortName/LongName]"
 * ========================================================================= */

#define MON_ID(octal, short_name, long_name) \
    "MON " octal " [" short_name "/" long_name "]"

/* Implemented MON call identifiers */
#define MON_ID_0B     MON_ID("0B", "LEAVE", "ExitFromProgram")
#define MON_ID_1B     MON_ID("1B", "INBT", "InByte")
#define MON_ID_2B     MON_ID("2B", "OUTBT", "OutByte")
#define MON_ID_3B     MON_ID("3B", "ECHOM", "SetEcho")
#define MON_ID_4B     MON_ID("4B", "BRKM", "SetBreak")
#define MON_ID_11B    MON_ID("11B", "TIME", "GetBasicTime")
#define MON_ID_12B    MON_ID("12B", "SETCM", "SetCommandBuffer")
#define MON_ID_13B    MON_ID("13B", "CIBUF", "ClearInBuffer")
#define MON_ID_16B    MON_ID("16B", "MGTTY", "GetTerminalType")
#define MON_ID_17B    MON_ID("17B", "MSTTY", "SetTerminalType")
#define MON_ID_22B    MON_ID("22B", "M8OUT", "OutUpTo8Bytes")
#define MON_ID_24B    MON_ID("24B", "B8OUT", "Out8Bytes")
#define MON_ID_30B    MON_ID("30B", "GETRT", "GetOwnRTAddress")
#define MON_ID_32B    MON_ID("32B", "MSG", "OutMessage")
#define MON_ID_35B    MON_ID("35B", "IOUT", "OutNumber")
#define MON_ID_41B    MON_ID("41B", "ROBJE", "ReadObjectEntry")
#define MON_ID_43B    MON_ID("43B", "CLOSE", "CloseFile")
#define MON_ID_50B    MON_ID("50B", "OPEN", "OpenFile")
#define MON_ID_54B    MON_ID("54B", "MDLFI", "DeleteFile")
#define MON_ID_62B    MON_ID("62B", "RMAX", "GetBytesInFile")
#define MON_ID_64B    MON_ID("64B", "ERMSG", "WarningMessage")
#define MON_ID_67B    MON_ID("67B", "OSIZE", "OutBufferSpace")
#define MON_ID_73B    MON_ID("73B", "SMAX", "SetMaxBytes")
#define MON_ID_74B    MON_ID("74B", "SETBT", "SetStartByte")
#define MON_ID_76B    MON_ID("76B", "SETBS", "SetBlockSize")
#define MON_ID_113B   MON_ID("113B", "CLOCK", "GetCurrentTime")
#define MON_ID_114B   MON_ID("114B", "TUSED", "GetTimeUsed")
#define MON_ID_117B   MON_ID("117B", "RFILE", "ReadFromFile")
#define MON_ID_120B   MON_ID("120B", "WFILE", "WriteToFile")
#define MON_ID_122B   MON_ID("122B", "RESRV", "ReserveResource")
#define MON_ID_123B   MON_ID("123B", "RELES", "ReleaseResource")
#define MON_ID_142B   MON_ID("142B", "ERMON", "ToErrorDevice")
#define MON_ID_143B   MON_ID("143B", "RSIO", "ExecutionInfo")
#define MON_ID_162B   MON_ID("162B", "OUTST", "OutString")
#define MON_ID_221B   MON_ID("221B", "CRALF", "CreateFile")
#define MON_ID_256B   MON_ID("256B", "DEABF", "FullFileName")
#define MON_ID_262B   MON_ID("262B", "CPUST", "GetSystemInfo")
#define MON_ID_312B   MON_ID("312B", "MOINF", "CheckMonCall")
#define MON_ID_317B   MON_ID("317B", "UECOM", "ExecuteCommand")
#define MON_ID_321B   MON_ID("321B", "UEADM", "UEAdministrator")
#define MON_ID_412B   MON_ID("412B", "FSCNT", "FileAsSegment")
#define MON_ID_413B   MON_ID("413B", "FSCDNT", "FileNotAsSegment")
#define MON_ID_422B   MON_ID("422B", "GSWSP", "GetScratchSegment")
#define MON_ID_503B   MON_ID("503B", "DVINST", "InputString")
#define MON_ID_504B   MON_ID("504B", "DVOUTS", "OutputString")

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
