/*
 * MON Parameter Access Helpers
 *
 * Functions for reading/writing MON call parameters.
 */

#include "mon.h"
#include <string.h>

/* =========================================================================
 * PARAMETER READ
 * ========================================================================= */

uint32_t mon_read_param_word(MonContext* ctx, int idx) {
    if (!ctx || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        return 0;
    }
    if (!ctx->read_word) {
        return 0;
    }
    return ctx->read_word(ctx->cpu, ctx->arg_addresses[idx]);
}

uint64_t mon_read_param_dword(MonContext* ctx, int idx) {
    if (!ctx || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        return 0;
    }
    if (!ctx->read_word) {
        return 0;
    }

    uint32_t addr = ctx->arg_addresses[idx];
    uint32_t low = ctx->read_word(ctx->cpu, addr);
    uint32_t high = ctx->read_word(ctx->cpu, addr + 4);
    return ((uint64_t)high << 32) | low;
}

uint8_t mon_read_param_byte(MonContext* ctx, int idx) {
    if (!ctx || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        return 0;
    }
    if (!ctx->read_byte) {
        return 0;
    }
    return ctx->read_byte(ctx->cpu, ctx->arg_addresses[idx]);
}

/* =========================================================================
 * PARAMETER WRITE
 * ========================================================================= */

void mon_write_param_word(MonContext* ctx, int idx, uint32_t value) {
    if (!ctx) {
        mon_log(MON_LOG_ERROR, "mon_write_param_word: ctx is NULL");
        return;
    }
    if (idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        mon_log(MON_LOG_ERROR, "mon_write_param_word: idx %d out of range (arg_count=%u)",
                idx, ctx->arg_count);
        return;
    }
    if (!ctx->write_word) {
        mon_log(MON_LOG_ERROR, "mon_write_param_word: write_word callback is NULL");
        return;
    }
    mon_log(MON_LOG_DEBUG, "mon_write_param_word: writing 0x%08X to addr 0x%08X (idx=%d)",
            value, ctx->arg_addresses[idx], idx);
    ctx->write_word(ctx->cpu, ctx->arg_addresses[idx], value);
}

void mon_write_param_halfword(MonContext* ctx, int idx, uint16_t value) {
    if (!ctx || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        return;
    }
    if (!ctx->write_halfword) {
        return;
    }
    uint32_t addr = ctx->arg_addresses[idx];
    mon_log(MON_LOG_DEBUG, "mon_write_param_halfword: idx=%d addr=0x%08X value=0x%04X",
            idx, addr, value);
    ctx->write_halfword(ctx->cpu, addr, value);
}

uint16_t mon_read_param_halfword(MonContext* ctx, int idx) {
    if (!ctx || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        return 0;
    }
    if (!ctx->read_halfword) {
        return 0;
    }
    return ctx->read_halfword(ctx->cpu, ctx->arg_addresses[idx]);
}

void mon_write_param_dword(MonContext* ctx, int idx, uint64_t value) {
    if (!ctx || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        return;
    }
    if (!ctx->write_word) {
        return;
    }

    uint32_t addr = ctx->arg_addresses[idx];
    ctx->write_word(ctx->cpu, addr, (uint32_t)(value & 0xFFFFFFFF));
    ctx->write_word(ctx->cpu, addr + 4, (uint32_t)(value >> 32));
}

void mon_write_param_byte(MonContext* ctx, int idx, uint8_t value) {
    if (!ctx || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        return;
    }
    if (!ctx->write_byte) {
        return;
    }
    ctx->write_byte(ctx->cpu, ctx->arg_addresses[idx], value);
}

/* =========================================================================
 * STRING HELPERS
 * ========================================================================= */

/**
 * Read a SINTRAN string directly from parameter address.
 * String is terminated by 0x00, 0x27 ('), or 0xFF.
 */
int mon_read_sintran_string(MonContext* ctx, int idx, char* buf, int max) {
    if (!ctx || !buf || max <= 0 || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        if (buf && max > 0) buf[0] = '\0';
        return 0;
    }
    if (!ctx->read_byte) {
        buf[0] = '\0';
        return 0;
    }

    uint32_t addr = ctx->arg_addresses[idx];
    int i;

    /* Read directly from address, terminated by 0x00, 0x27, or 0xFF */
    for (i = 0; i < max - 1; i++) {
        uint8_t ch = ctx->read_byte(ctx->cpu, addr + i);
        if (ch == 0x00 || ch == 0x27 || ch == 0xFF) {
            break;
        }
        buf[i] = (char)ch;
    }
    buf[i] = '\0';

    mon_log(MON_LOG_DEBUG, "mon_read_sintran_string: addr=0x%08X len=%d str='%s'",
            addr, i, buf);

    return i;
}

/**
 * Read a string from a [Length:4][Pointer:4] descriptor.
 * Used by FORTRAN-500 and Pascal compilers.
 * Returns -1 if descriptor format is invalid.
 */
int mon_read_descriptor_string(MonContext* ctx, int idx, char* buf, int max) {
    if (!ctx || !buf || max <= 0 || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        if (buf && max > 0) buf[0] = '\0';
        return -1;
    }
    if (!ctx->read_byte || !ctx->read_word) {
        buf[0] = '\0';
        return -1;
    }

    uint32_t addr = ctx->arg_addresses[idx];

    /* Read descriptor: [Length:4][Pointer:4] */
    uint32_t str_len = ctx->read_word(ctx->cpu, addr);
    uint32_t str_ptr = ctx->read_word(ctx->cpu, addr + 4);

    mon_log(MON_LOG_DEBUG, "mon_read_descriptor_string: addr=0x%08X desc=[len=%u, ptr=0x%08X]",
            addr, str_len, str_ptr);

    /* Validate descriptor - length must be reasonable, pointer must be non-zero */
    if (str_len == 0 || str_len > 10000 || str_ptr == 0) {
        mon_log(MON_LOG_WARN, "mon_read_descriptor_string: invalid descriptor at 0x%08X",
                addr);
        buf[0] = '\0';
        return -1;
    }

    /* Limit to buffer size */
    int read_len = (int)str_len;
    if (read_len > max - 1) {
        read_len = max - 1;
    }

    /* Read string from pointer, checking for terminators */
    int i;
    for (i = 0; i < read_len; i++) {
        uint8_t ch = ctx->read_byte(ctx->cpu, str_ptr + i);
        /* SINTRAN string terminators: 0x00, 0x27 (apostrophe), or 0xFF */
        if (ch == 0x00 || ch == 0x27 || ch == 0xFF) {
            break;
        }
        buf[i] = (char)ch;
    }
    buf[i] = '\0';

    /* Trim trailing spaces (common in fixed-length strings) */
    while (i > 0 && buf[i-1] == ' ') {
        buf[--i] = '\0';
    }

    mon_log(MON_LOG_DEBUG, "mon_read_descriptor_string: result len=%d str='%s'", i, buf);

    return i;
}

/**
 * DEPRECATED: Auto-detect string format.
 * Use mon_read_sintran_string or mon_read_descriptor_string instead.
 */
int mon_read_string(MonContext* ctx, int idx, char* buf, int max) {
    if (!ctx || !buf || max <= 0 || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        if (buf && max > 0) buf[0] = '\0';
        return 0;
    }
    if (!ctx->read_byte || !ctx->read_word) {
        buf[0] = '\0';
        return 0;
    }

    uint32_t addr = ctx->arg_addresses[idx];

    /* Debug: dump first 16 bytes at argument address */
    uint32_t word0 = ctx->read_word(ctx->cpu, addr);
    uint32_t word1 = ctx->read_word(ctx->cpu, addr + 4);
    mon_log(MON_LOG_DEBUG, "mon_read_string(DEPRECATED): addr=0x%08X, words: [0x%08X, 0x%08X]",
            addr, word0, word1);

    /* Try descriptor format first - check if word1 looks like a valid pointer */
    uint8_t ptr_segment = (word1 >> 24) & 0xFF;
    int is_valid_segment = (ptr_segment == 0x08 || ptr_segment == 0x10 ||
                            ptr_segment == 0x18 || ptr_segment == 0x20);

    if (word0 > 0 && word0 < 1000 && is_valid_segment) {
        int result = mon_read_descriptor_string(ctx, idx, buf, max);
        if (result >= 0) {
            return result;
        }
    }

    /* Fall back to direct SINTRAN string */
    return mon_read_sintran_string(ctx, idx, buf, max);
}

int mon_write_string(MonContext* ctx, int idx, const char* str) {
    if (!ctx || !str || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        return 0;
    }
    if (!ctx->write_byte) {
        return 0;
    }

    uint32_t addr = ctx->arg_addresses[idx];
    int len = (int)strlen(str);

    for (int i = 0; i < len; i++) {
        ctx->write_byte(ctx->cpu, addr + i, (uint8_t)str[i]);
    }
    /* Null terminate */
    ctx->write_byte(ctx->cpu, addr + len, 0x00);

    return len;
}

/* =========================================================================
 * ERROR HANDLING HELPERS
 *
 * These functions set the generic error_code and error_flag fields in
 * MonContext. The CPU-specific code (nd500_indirect.c or nd100 equivalent)
 * reads these and applies them to the appropriate CPU registers/flags.
 *
 * For ND-500: error_flag -> K flag, error_code -> W1 (I1) register
 * For ND-100: error_flag -> K flag, error_code -> A register (typically)
 * ========================================================================= */

void mon_set_error(MonContext* ctx, int32_t error_code) {
    if (!ctx) return;

    /* Store error code and flag in context (CPU-agnostic) */
    ctx->error_flag = 1;
    ctx->error_code = error_code;

    /* Also call CPU-specific callbacks if provided (for backwards compat) */
    if (ctx->set_k_flag) {
        ctx->set_k_flag(ctx->cpu, 1);
    }

    /* Note: error_code is stored in context; CPU code should read it
     * from ctx->error_code and place it in the appropriate register.
     * The set_error_code callback is deprecated but still called for compat. */
    if (ctx->set_error_code) {
        ctx->set_error_code(ctx->cpu, error_code);
    }
}

void mon_set_success(MonContext* ctx) {
    if (!ctx) {
        mon_log(MON_LOG_ERROR, "mon_set_success: ctx is NULL");
        return;
    }

    /* Clear error flag and code */
    ctx->error_flag = 0;
    ctx->error_code = 0;

    /* Call CPU-specific callback if provided */
    if (ctx->set_k_flag) {
        mon_log(MON_LOG_DEBUG, "mon_set_success: clearing K flag (K=0)");
        ctx->set_k_flag(ctx->cpu, 0);
    } else {
        mon_log(MON_LOG_ERROR, "mon_set_success: set_k_flag callback is NULL!");
    }
}

void mon_request_halt(MonContext* ctx, const char* reason) {
    if (!ctx) return;

    ctx->halt_requested = 1;
    ctx->halt_reason = reason;
}
