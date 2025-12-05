/*
 * SINTRAN III Monitor Call Logging Implementation
 */

#include "mon_log.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* =========================================================================
 * GLOBAL STATE
 * ========================================================================= */

static int g_log_enabled = 0;
static MonLogLevel g_log_level = MON_LOG_INFO;
static MonLogCallback g_log_callback = NULL;

/* Buffer for formatting log messages */
#define LOG_BUFFER_SIZE 2048
static char g_log_buffer[LOG_BUFFER_SIZE];

/* =========================================================================
 * CONFIGURATION API
 * ========================================================================= */

void mon_log_enable(int enabled) {
    g_log_enabled = enabled ? 1 : 0;
}

int mon_log_is_enabled(void) {
    return g_log_enabled;
}

void mon_log_set_level(MonLogLevel level) {
    g_log_level = level;
}

MonLogLevel mon_log_get_level(void) {
    return g_log_level;
}

void mon_log_set_callback(MonLogCallback callback) {
    g_log_callback = callback;
}

/* =========================================================================
 * INTERNAL HELPERS
 * ========================================================================= */

static const char* level_prefix(MonLogLevel level) {
    switch (level) {
        case MON_LOG_ERROR: return "[MON:ERROR]";
        case MON_LOG_WARN:  return "[MON:WARN ]";
        case MON_LOG_INFO:  return "[MON:INFO ]";
        case MON_LOG_DEBUG: return "[MON:DEBUG]";
        case MON_LOG_TRACE: return "[MON:TRACE]";
        default:            return "[MON]";
    }
}

static const char* io_direction(MonParamIO io) {
    switch (io) {
        case MON_PARAM_IN:    return "IN ";
        case MON_PARAM_OUT:   return "OUT";
        case MON_PARAM_INOUT: return "I/O";
        default:              return "???";
    }
}

/* Output a log message */
static void emit_log(MonLogLevel level, const char* message) {
    if (!g_log_enabled || level > g_log_level) {
        return;
    }

    if (g_log_callback) {
        g_log_callback(level, message);
    } else {
        fprintf(stderr, "%s %s\n", level_prefix(level), message);
    }
}

/* =========================================================================
 * LOGGING FUNCTIONS
 * ========================================================================= */

void mon_log(MonLogLevel level, const char* fmt, ...) {
    if (!g_log_enabled || level > g_log_level) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(g_log_buffer, LOG_BUFFER_SIZE, fmt, args);
    va_end(args);

    emit_log(level, g_log_buffer);
}

void mon_logv(MonLogLevel level, const char* fmt, va_list args) {
    if (!g_log_enabled || level > g_log_level) {
        return;
    }

    vsnprintf(g_log_buffer, LOG_BUFFER_SIZE, fmt, args);
    emit_log(level, g_log_buffer);
}

/* =========================================================================
 * MON CALL ENTRY/EXIT LOGGING
 * ========================================================================= */

void mon_log_entry(MonContext* ctx, const char* name) {
    if (!g_log_enabled || g_log_level < MON_LOG_INFO) {
        return;
    }

    /* Convert MON number to octal string */
    char octal[16];
    snprintf(octal, sizeof(octal), "%oB", ctx->mon_number);

    snprintf(g_log_buffer, LOG_BUFFER_SIZE,
             "CALL %s %s (%u args) from PC=0x%08X",
             octal, name ? name : "???",
             ctx->arg_count,
             ctx->return_address);
    emit_log(MON_LOG_INFO, g_log_buffer);

    /* At DEBUG level, also log argument addresses */
    if (g_log_level >= MON_LOG_DEBUG && ctx->arg_count > 0) {
        char* p = g_log_buffer;
        int remaining = LOG_BUFFER_SIZE;
        int n = snprintf(p, remaining, "  Arg addresses: [");
        p += n;
        remaining -= n;

        for (uint32_t i = 0; i < ctx->arg_count && i < 8 && remaining > 20; i++) {
            n = snprintf(p, remaining, "%s0x%08X",
                         i > 0 ? ", " : "",
                         ctx->arg_addresses[i]);
            p += n;
            remaining -= n;
        }
        if (ctx->arg_count > 8) {
            snprintf(p, remaining, ", ...]");
        } else {
            snprintf(p, remaining, "]");
        }
        emit_log(MON_LOG_DEBUG, g_log_buffer);
    }
}

void mon_log_exit(MonContext* ctx, const char* name, MonResult result) {
    if (!g_log_enabled || g_log_level < MON_LOG_INFO) {
        return;
    }

    char octal[16];
    snprintf(octal, sizeof(octal), "%oB", ctx->mon_number);

    const char* result_str = (result == MON_SUCCESS) ? "SUCCESS" : "ERROR";

    snprintf(g_log_buffer, LOG_BUFFER_SIZE,
             "EXIT %s %s -> %s%s",
             octal, name ? name : "???",
             result_str,
             ctx->halt_requested ? " (HALT)" : "");
    emit_log(MON_LOG_INFO, g_log_buffer);
}

/* =========================================================================
 * PARAMETER LOGGING
 * ========================================================================= */

void mon_log_param_word(MonContext* ctx, int idx, const char* name, MonParamIO io) {
    if (!g_log_enabled || g_log_level < MON_LOG_DEBUG) {
        return;
    }

    if (idx < 0 || (uint32_t)idx >= ctx->arg_count || !ctx->read_word) {
        snprintf(g_log_buffer, LOG_BUFFER_SIZE,
                 "  %s %s: <invalid arg index %d>",
                 io_direction(io), name ? name : "???", idx);
        emit_log(MON_LOG_DEBUG, g_log_buffer);
        return;
    }

    uint32_t addr = ctx->arg_addresses[idx];
    uint32_t value = ctx->read_word(ctx->cpu, addr);

    snprintf(g_log_buffer, LOG_BUFFER_SIZE,
             "  %s %s: 0x%08X (%d) @ 0x%08X",
             io_direction(io), name ? name : "???",
             value, (int32_t)value, addr);
    emit_log(MON_LOG_DEBUG, g_log_buffer);
}

void mon_log_param_byte(MonContext* ctx, int idx, const char* name, MonParamIO io) {
    if (!g_log_enabled || g_log_level < MON_LOG_DEBUG) {
        return;
    }

    if (idx < 0 || (uint32_t)idx >= ctx->arg_count || !ctx->read_byte) {
        snprintf(g_log_buffer, LOG_BUFFER_SIZE,
                 "  %s %s: <invalid arg index %d>",
                 io_direction(io), name ? name : "???", idx);
        emit_log(MON_LOG_DEBUG, g_log_buffer);
        return;
    }

    uint32_t addr = ctx->arg_addresses[idx];
    uint8_t value = ctx->read_byte(ctx->cpu, addr);

    /* Show printable character if applicable */
    char char_repr[8] = "";
    if (isprint(value)) {
        snprintf(char_repr, sizeof(char_repr), " '%c'", value);
    }

    snprintf(g_log_buffer, LOG_BUFFER_SIZE,
             "  %s %s: 0x%02X (%d)%s @ 0x%08X",
             io_direction(io), name ? name : "???",
             value, value, char_repr, addr);
    emit_log(MON_LOG_DEBUG, g_log_buffer);
}

void mon_log_param_dword(MonContext* ctx, int idx, const char* name, MonParamIO io) {
    if (!g_log_enabled || g_log_level < MON_LOG_DEBUG) {
        return;
    }

    if (idx < 0 || (uint32_t)idx >= ctx->arg_count || !ctx->read_word) {
        snprintf(g_log_buffer, LOG_BUFFER_SIZE,
                 "  %s %s: <invalid arg index %d>",
                 io_direction(io), name ? name : "???", idx);
        emit_log(MON_LOG_DEBUG, g_log_buffer);
        return;
    }

    uint32_t addr = ctx->arg_addresses[idx];
    uint32_t low = ctx->read_word(ctx->cpu, addr);
    uint32_t high = ctx->read_word(ctx->cpu, addr + 4);
    uint64_t value = ((uint64_t)high << 32) | low;

    snprintf(g_log_buffer, LOG_BUFFER_SIZE,
             "  %s %s: 0x%016llX (%lld) @ 0x%08X",
             io_direction(io), name ? name : "???",
             (unsigned long long)value, (long long)value, addr);
    emit_log(MON_LOG_DEBUG, g_log_buffer);
}

void mon_log_param_string(MonContext* ctx, int idx, const char* name, MonParamIO io) {
    if (!g_log_enabled || g_log_level < MON_LOG_DEBUG) {
        return;
    }

    if (idx < 0 || (uint32_t)idx >= ctx->arg_count || !ctx->read_byte) {
        snprintf(g_log_buffer, LOG_BUFFER_SIZE,
                 "  %s %s: <invalid arg index %d>",
                 io_direction(io), name ? name : "???", idx);
        emit_log(MON_LOG_DEBUG, g_log_buffer);
        return;
    }

    /* Read string from memory (SINTRAN string descriptor format) */
    uint32_t addr = ctx->arg_addresses[idx];

    /* Read up to 64 chars for logging */
    char str_preview[65];
    int i;
    for (i = 0; i < 64; i++) {
        uint8_t ch = ctx->read_byte(ctx->cpu, addr + i);
        if (ch == 0 || ch == 0xFF) break;  /* End markers */
        str_preview[i] = isprint(ch) ? (char)ch : '.';
    }
    str_preview[i] = '\0';

    snprintf(g_log_buffer, LOG_BUFFER_SIZE,
             "  %s %s: \"%s\"%s @ 0x%08X",
             io_direction(io), name ? name : "???",
             str_preview,
             i >= 64 ? "..." : "",
             addr);
    emit_log(MON_LOG_DEBUG, g_log_buffer);
}

void mon_log_param_buffer(MonContext* ctx, uint32_t addr, int len, const char* name, MonParamIO io) {
    if (!g_log_enabled || g_log_level < MON_LOG_TRACE) {
        return;
    }

    if (!ctx->read_byte) {
        snprintf(g_log_buffer, LOG_BUFFER_SIZE,
                 "  %s %s: <no read callback>",
                 io_direction(io), name ? name : "???");
        emit_log(MON_LOG_TRACE, g_log_buffer);
        return;
    }

    snprintf(g_log_buffer, LOG_BUFFER_SIZE,
             "  %s %s (%d bytes) @ 0x%08X:",
             io_direction(io), name ? name : "???", len, addr);
    emit_log(MON_LOG_TRACE, g_log_buffer);

    /* Hex dump (up to 256 bytes) */
    int dump_len = len > 256 ? 256 : len;
    for (int offset = 0; offset < dump_len; offset += 16) {
        char* p = g_log_buffer;
        int remaining = LOG_BUFFER_SIZE;

        int n = snprintf(p, remaining, "    %04X: ", offset);
        p += n;
        remaining -= n;

        /* Hex bytes */
        for (int i = 0; i < 16 && (offset + i) < dump_len; i++) {
            uint8_t byte = ctx->read_byte(ctx->cpu, addr + offset + i);
            n = snprintf(p, remaining, "%02X ", byte);
            p += n;
            remaining -= n;
        }

        /* ASCII representation */
        n = snprintf(p, remaining, " |");
        p += n;
        remaining -= n;

        for (int i = 0; i < 16 && (offset + i) < dump_len; i++) {
            uint8_t byte = ctx->read_byte(ctx->cpu, addr + offset + i);
            n = snprintf(p, remaining, "%c", isprint(byte) ? byte : '.');
            p += n;
            remaining -= n;
        }
        snprintf(p, remaining, "|");

        emit_log(MON_LOG_TRACE, g_log_buffer);
    }

    if (len > 256) {
        snprintf(g_log_buffer, LOG_BUFFER_SIZE, "    ... (%d more bytes)", len - 256);
        emit_log(MON_LOG_TRACE, g_log_buffer);
    }
}
