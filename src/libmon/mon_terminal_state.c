/*
 * Terminal State Management Implementation
 *
 * Manages per-device break/echo settings for SINTRAN III terminal I/O.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "mon_terminal_state.h"
#include "mon.h"
#include <string.h>

/* Static state for all devices */
static TerminalState terminal_states[MAX_TERMINAL_DEVICES];

/* ============================================================
 * BitTable128 Operations
 * ============================================================ */

void bit_table_clear(BitTable128* table) {
    memset(table->bits, 0, sizeof(table->bits));
}

void bit_table_set_bit(BitTable128* table, uint8_t bit_num) {
    if (bit_num >= 128) return;
    int byte_idx = bit_num / 8;
    int bit_idx = 7 - (bit_num % 8);  /* MSB first within byte */
    table->bits[byte_idx] |= (1 << bit_idx);
}

void bit_table_clear_bit(BitTable128* table, uint8_t bit_num) {
    if (bit_num >= 128) return;
    int byte_idx = bit_num / 8;
    int bit_idx = 7 - (bit_num % 8);
    table->bits[byte_idx] &= ~(1 << bit_idx);
}

bool bit_table_test_bit(const BitTable128* table, uint8_t bit_num) {
    if (bit_num >= 128) return false;
    int byte_idx = bit_num / 8;
    int bit_idx = 7 - (bit_num % 8);
    return (table->bits[byte_idx] & (1 << bit_idx)) != 0;
}

void bit_table_from_words(BitTable128* table, const uint32_t* words, int word_count) {
    bit_table_clear(table);
    /* 128 bits = 4 x 32-bit words, big-endian format */
    for (int i = 0; i < word_count && i < 4; i++) {
        uint32_t w = words[i];
        table->bits[i * 4 + 0] = (w >> 24) & 0xFF;
        table->bits[i * 4 + 1] = (w >> 16) & 0xFF;
        table->bits[i * 4 + 2] = (w >> 8) & 0xFF;
        table->bits[i * 4 + 3] = w & 0xFF;
    }
}

/* ============================================================
 * Predefined Break Tables
 * ============================================================ */

void mon_get_control_break_table(BitTable128* table) {
    /* Strategy 1: All control characters (0-31) are break characters */
    bit_table_clear(table);
    for (int i = 0; i < 32; i++) {
        bit_table_set_bit(table, (uint8_t)i);
    }
}

void mon_get_mac_break_table(BitTable128* table) {
    /* Strategy 2: MAC - CR, LF, ESC, EOF, and SINTRAN string end */
    bit_table_clear(table);
    bit_table_set_bit(table, BREAK_CHAR_CR);   /* 0x0D */
    bit_table_set_bit(table, BREAK_CHAR_LF);   /* 0x0A */
    bit_table_set_bit(table, BREAK_CHAR_ESC);  /* 0x1B */
    bit_table_set_bit(table, BREAK_CHAR_EOF);  /* 0x04 */
    bit_table_set_bit(table, BREAK_CHAR_END);  /* 0x27 */
}

/* ============================================================
 * Predefined Echo Tables
 * ============================================================ */

void mon_get_no_control_echo_table(BitTable128* table) {
    /* Strategy 1: Echo all except control characters */
    /* Set bits for characters 32-127 (printable ASCII) */
    bit_table_clear(table);
    for (int i = 32; i < 128; i++) {
        bit_table_set_bit(table, (uint8_t)i);
    }
}

/* ============================================================
 * Initialization
 * ============================================================ */

void mon_terminal_state_init(void) {
    memset(terminal_states, 0, sizeof(terminal_states));
    mon_log(MON_LOG_INFO, "Terminal state table initialized");
}

void mon_terminal_state_reset(void) {
    mon_terminal_state_init();
}

/* ============================================================
 * Terminal State Access
 * ============================================================ */

TerminalState* mon_terminal_state_get(uint32_t device_no) {
    if (device_no >= MAX_TERMINAL_DEVICES) {
        return NULL;
    }
    return &terminal_states[device_no];
}

/* ============================================================
 * SetBreak (MON 4B)
 * ============================================================ */

int mon_set_break_strategy(uint32_t device_no, int32_t strategy,
                           const uint32_t* user_table, uint32_t max_chars) {
    if (device_no >= MAX_TERMINAL_DEVICES) {
        mon_log(MON_LOG_WARN, "SetBreak: Device %u out of range", device_no);
        return -52;  /* Invalid parameter */
    }

    TerminalState* state = &terminal_states[device_no];
    state->initialized = true;
    state->break_strategy = strategy;
    state->max_chars = max_chars;

    /* For strategy 7 (user-defined), copy the user table.
     * Strategy 8 uses the same table without resending. */
    if (strategy == BREAK_STRAT_USER && user_table != NULL) {
        /* Load new user table (4 x 32-bit words = 128 bits) */
        bit_table_from_words(&state->user_break_table, user_table, 4);
    }

    mon_log(MON_LOG_DEBUG, "SetBreak: Device %u strategy=%d max_chars=%u",
            device_no, strategy, max_chars);
    return 0;
}

/* ============================================================
 * SetEcho (MON 3B)
 * ============================================================ */

int mon_set_echo_strategy(uint32_t device_no, int32_t strategy,
                          const uint32_t* user_table) {
    if (device_no >= MAX_TERMINAL_DEVICES) {
        mon_log(MON_LOG_WARN, "SetEcho: Device %u out of range", device_no);
        return -52;  /* Invalid parameter */
    }

    TerminalState* state = &terminal_states[device_no];
    state->initialized = true;
    state->echo_strategy = strategy;

    /* For strategy 7 (user-defined), copy the user table */
    if (strategy == ECHO_STRAT_USER && user_table != NULL) {
        bit_table_from_words(&state->user_echo_table, user_table, 4);
    }

    mon_log(MON_LOG_DEBUG, "SetEcho: Device %u strategy=%d",
            device_no, strategy);
    return 0;
}

/* ============================================================
 * Break Character Testing
 * ============================================================ */

bool mon_is_break_char(uint32_t device_no, uint8_t ch) {
    TerminalState* state = (device_no < MAX_TERMINAL_DEVICES)
                            ? &terminal_states[device_no]
                            : NULL;

    /* 8-bit I/O mode special handling:
     * If enabled and character is >= 128, check if bit 7 (BEL char) is set
     * in the break table. If so, all high-bit characters cause break. */
    if (state && state->eight_bit_io && ch >= 128) {
        if (bit_table_test_bit(&state->user_break_table, 7)) {
            return true;  /* All high-bit chars break */
        }
        /* Otherwise, high-bit chars don't break (fall through to false) */
        return false;
    }

    /* Use MAC as default if no state initialized */
    int32_t strategy = (state && state->initialized)
                        ? state->break_strategy
                        : BREAK_STRAT_MAC;

    switch (strategy) {
        case BREAK_STRAT_NONE:
            /* No break characters */
            return false;

        case BREAK_STRAT_ALL:
            /* All characters break */
            return true;

        case BREAK_STRAT_CONTROL:
            /* Control characters (0-31) break */
            return ch < 32;

        case BREAK_STRAT_MAC:
            /* MAC: CR, LF, ESC, EOF, SINTRAN end */
            return (ch == BREAK_CHAR_CR || ch == BREAK_CHAR_LF ||
                    ch == BREAK_CHAR_ESC || ch == BREAK_CHAR_EOF ||
                    ch == BREAK_CHAR_END);

        case BREAK_STRAT_SYSTEM_3:
        case BREAK_STRAT_SYSTEM_4:
        case BREAK_STRAT_SYSTEM_5:
        case BREAK_STRAT_SYSTEM_6:
            /* System tables - PLACEHOLDER: use MAC as fallback */
            return (ch == BREAK_CHAR_CR || ch == BREAK_CHAR_LF ||
                    ch == BREAK_CHAR_ESC || ch == BREAK_CHAR_EOF ||
                    ch == BREAK_CHAR_END);

        case BREAK_STRAT_USER:
        case BREAK_STRAT_LAST_USER:
            /* User-defined table (7) and last user table (8) use same table */
            if (state) {
                return bit_table_test_bit(&state->user_break_table, ch);
            }
            return false;

        case BREAK_STRAT_MAX_ONLY:
            /* Strategy 9: No character breaks - only max_chars count */
            return false;

        default:
            /* Unknown strategy - use MAC */
            return (ch == BREAK_CHAR_CR || ch == BREAK_CHAR_LF ||
                    ch == BREAK_CHAR_ESC || ch == BREAK_CHAR_EOF ||
                    ch == BREAK_CHAR_END);
    }
}

/* ============================================================
 * Echo Character Testing
 * ============================================================ */

bool mon_should_echo_char(uint32_t device_no, uint8_t ch) {
    TerminalState* state = (device_no < MAX_TERMINAL_DEVICES)
                            ? &terminal_states[device_no]
                            : NULL;

    /* 8-bit I/O mode special handling:
     * If enabled and character is >= 128, check if bit 7 (BEL char) is set
     * in the echo table. If so, all high-bit characters are echoed. */
    if (state && state->eight_bit_io && ch >= 128) {
        if (bit_table_test_bit(&state->user_echo_table, 7)) {
            return true;  /* All high-bit chars echoed */
        }
        /* Otherwise, high-bit chars not echoed */
        return false;
    }

    /* Use "echo all" as default if no state initialized */
    int32_t strategy = (state && state->initialized)
                        ? state->echo_strategy
                        : ECHO_STRAT_ALL;

    switch (strategy) {
        case ECHO_STRAT_NONE:
            /* No echo */
            return false;

        case ECHO_STRAT_ALL:
            /* Echo all characters */
            return true;

        case ECHO_STRAT_NO_CONTROL:
            /* Echo except control characters */
            return ch >= 32 && ch < 127;

        case ECHO_STRAT_MAC:
            /* MAC echo - echo printable and some control chars */
            return (ch >= 32 && ch < 127) ||
                   ch == BREAK_CHAR_CR || ch == BREAK_CHAR_LF;

        case ECHO_STRAT_SYSTEM_3:
        case ECHO_STRAT_SYSTEM_4:
        case ECHO_STRAT_SYSTEM_5:
        case ECHO_STRAT_SYSTEM_6:
            /* System tables - PLACEHOLDER: use "echo all except control" */
            return ch >= 32 && ch < 127;

        case ECHO_STRAT_USER:
        case ECHO_STRAT_LAST_USER:
            /* User-defined table (7) and last user table (8) use same table.
             * IMPORTANT: Echo table has INVERTED semantics from break table!
             * bit=0 means echo, bit=1 means don't echo. */
            if (state) {
                return !bit_table_test_bit(&state->user_echo_table, ch);
            }
            return true;

        default:
            /* Unknown strategy - echo all */
            return true;
    }
}

/* ============================================================
 * Strategy Getters
 * ============================================================ */

int32_t mon_get_break_strategy(uint32_t device_no) {
    if (device_no >= MAX_TERMINAL_DEVICES) {
        return BREAK_STRAT_MAC;  /* Default */
    }
    TerminalState* state = &terminal_states[device_no];
    return state->initialized ? state->break_strategy : BREAK_STRAT_MAC;
}

int32_t mon_get_echo_strategy(uint32_t device_no) {
    if (device_no >= MAX_TERMINAL_DEVICES) {
        return ECHO_STRAT_ALL;  /* Default */
    }
    TerminalState* state = &terminal_states[device_no];
    return state->initialized ? state->echo_strategy : ECHO_STRAT_ALL;
}

uint32_t mon_get_max_chars(uint32_t device_no) {
    if (device_no >= MAX_TERMINAL_DEVICES) {
        return 0;  /* No limit */
    }
    TerminalState* state = &terminal_states[device_no];
    return state->max_chars;
}

/* ============================================================
 * User Table Management (for DVINST)
 * ============================================================ */

void mon_update_user_break_table(uint32_t device_no, const uint32_t* words) {
    if (device_no >= MAX_TERMINAL_DEVICES || words == NULL) {
        return;
    }
    TerminalState* state = &terminal_states[device_no];
    state->initialized = true;
    bit_table_from_words(&state->user_break_table, words, 4);
}

void mon_update_user_echo_table(uint32_t device_no, const uint32_t* words) {
    if (device_no >= MAX_TERMINAL_DEVICES || words == NULL) {
        return;
    }
    TerminalState* state = &terminal_states[device_no];
    state->initialized = true;
    bit_table_from_words(&state->user_echo_table, words, 4);
}

const BitTable128* mon_get_user_break_table(uint32_t device_no) {
    if (device_no >= MAX_TERMINAL_DEVICES) {
        return NULL;
    }
    TerminalState* state = &terminal_states[device_no];
    return &state->user_break_table;
}

const BitTable128* mon_get_user_echo_table(uint32_t device_no) {
    if (device_no >= MAX_TERMINAL_DEVICES) {
        return NULL;
    }
    TerminalState* state = &terminal_states[device_no];
    return &state->user_echo_table;
}

/* ============================================================
 * 8-bit I/O Mode (TerminalFunction 112)
 * ============================================================ */

void mon_set_eight_bit_io(uint32_t device_no, bool enabled) {
    if (device_no >= MAX_TERMINAL_DEVICES) {
        return;
    }
    TerminalState* state = &terminal_states[device_no];
    state->initialized = true;
    state->eight_bit_io = enabled;
    mon_log(MON_LOG_DEBUG, "8-bit I/O mode %s for device %u",
            enabled ? "enabled" : "disabled", device_no);
}

bool mon_get_eight_bit_io(uint32_t device_no) {
    if (device_no >= MAX_TERMINAL_DEVICES) {
        return false;
    }
    TerminalState* state = &terminal_states[device_no];
    return state->eight_bit_io;
}
