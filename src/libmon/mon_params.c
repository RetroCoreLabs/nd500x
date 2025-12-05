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
    if (!ctx || idx < 0 || (uint32_t)idx >= ctx->arg_count) {
        return;
    }
    if (!ctx->write_word) {
        return;
    }
    ctx->write_word(ctx->cpu, ctx->arg_addresses[idx], value);
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

int mon_read_string(MonContext* ctx, int idx, char* buf, int max) {
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

    for (i = 0; i < max - 1; i++) {
        uint8_t ch = ctx->read_byte(ctx->cpu, addr + i);
        /* SINTRAN string terminators: 0x00 or 0xFF */
        if (ch == 0x00 || ch == 0xFF) {
            break;
        }
        buf[i] = (char)ch;
    }
    buf[i] = '\0';

    return i;
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
 * ========================================================================= */

void mon_set_error(MonContext* ctx, int32_t error_code) {
    if (!ctx) return;

    /* Set K flag = 1 (error) */
    if (ctx->set_k_flag) {
        ctx->set_k_flag(ctx->cpu, 1);
    }

    /* Set error code in I1 register */
    if (ctx->set_error_code) {
        ctx->set_error_code(ctx->cpu, error_code);
    } else if (ctx->set_i1) {
        ctx->set_i1(ctx->cpu, (uint32_t)error_code);
    }
}

void mon_set_success(MonContext* ctx) {
    if (!ctx) return;

    /* Set K flag = 0 (success) */
    if (ctx->set_k_flag) {
        ctx->set_k_flag(ctx->cpu, 0);
    }
}

void mon_request_halt(MonContext* ctx, const char* reason) {
    if (!ctx) return;

    ctx->halt_requested = 1;
    ctx->halt_reason = reason;
}
