/*
 * Terminal State Management for SINTRAN III MON Calls
 *
 * Manages per-device break/echo settings for terminal I/O operations.
 * Used by SetBreak (MON 4B), SetEcho (MON 3B), and DVINST (MON 503B).
 *
 * Break Strategy Values (from SINTRAN III Reference Manual):
 *   <0: No break - input continues until max chars or EOF
 *    0: All characters break - every character terminates input
 *    1: Control characters (0-31) break
 *    2: MAC (machine code) - CR, LF, ESC, EOF
 *  3-6: System-defined tables
 *    7: User-defined 128-bit table (each bit = one ASCII character)
 *    8: Last user-defined table (don't resend)
 *    9: Max chars only - break only when max_chars reached
 *
 * Echo Strategy Values:
 *   <0: No echo - characters not displayed
 *    0: Echo all characters
 *    1: Echo except control characters
 *    2: MAC echo strategy
 *  3-6: System-defined tables
 *    7: User-defined 128-bit table (bit=0 means echo, INVERTED from break!)
 *    8: Last user-defined table (don't resend)
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 * Reference: SINTRAN III Reference Manual (ND-60.128.03)
 */

#ifndef MON_TERMINAL_STATE_H
#define MON_TERMINAL_STATE_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum number of devices with terminal state */
#define MAX_TERMINAL_DEVICES 128

/* Break strategy constants */
#define BREAK_STRAT_NONE      (-1)  /* No break characters */
#define BREAK_STRAT_ALL       0     /* All characters break */
#define BREAK_STRAT_CONTROL   1     /* Control characters (0-31) break */
#define BREAK_STRAT_MAC       2     /* MAC: CR, LF, ESC, EOF */
#define BREAK_STRAT_SYSTEM_3  3     /* System-defined table 3 */
#define BREAK_STRAT_SYSTEM_4  4     /* System-defined table 4 */
#define BREAK_STRAT_SYSTEM_5  5     /* System-defined table 5 */
#define BREAK_STRAT_SYSTEM_6  6     /* System-defined table 6 */
#define BREAK_STRAT_USER      7     /* User-defined 128-bit table */
#define BREAK_STRAT_LAST_USER 8     /* Use last user table */
#define BREAK_STRAT_MAX_ONLY  9     /* Break only on max chars */

/* Echo strategy constants */
#define ECHO_STRAT_NONE       (-1)  /* No echo */
#define ECHO_STRAT_ALL        0     /* Echo all characters */
#define ECHO_STRAT_NO_CONTROL 1     /* Echo except control chars */
#define ECHO_STRAT_MAC        2     /* MAC echo strategy */
#define ECHO_STRAT_SYSTEM_3   3     /* System-defined table 3 */
#define ECHO_STRAT_SYSTEM_4   4     /* System-defined table 4 */
#define ECHO_STRAT_SYSTEM_5   5     /* System-defined table 5 */
#define ECHO_STRAT_SYSTEM_6   6     /* System-defined table 6 */
#define ECHO_STRAT_USER       7     /* User-defined 128-bit table */
#define ECHO_STRAT_LAST_USER  8     /* Use last user table */

/* Default break characters for MAC strategy */
#define BREAK_CHAR_CR   0x0D  /* Carriage return */
#define BREAK_CHAR_LF   0x0A  /* Line feed */
#define BREAK_CHAR_ESC  0x1B  /* Escape */
#define BREAK_CHAR_EOF  0x04  /* Ctrl-D (EOF) */
#define BREAK_CHAR_END  0x27  /* SINTRAN string terminator */

/* 128-bit table (16 bytes = 8 words) */
typedef struct {
    uint8_t bits[16];  /* 128 bits, one per ASCII character 0-127 */
} BitTable128;

/* Terminal state for a single device */
typedef struct {
    bool initialized;           /* True if state has been set */
    int32_t break_strategy;     /* Current break strategy */
    int32_t echo_strategy;      /* Current echo strategy */
    uint32_t max_chars;         /* Maximum characters before auto-break */
    bool eight_bit_io;          /* 8-bit I/O mode (TerminalFunction 112) */
    BitTable128 user_break_table;  /* User-defined break table (strategy 7 AND 8) */
    BitTable128 user_echo_table;   /* User-defined echo table (strategy 7 AND 8) */
} TerminalState;

/* Initialization */
void mon_terminal_state_init(void);
void mon_terminal_state_reset(void);

/* Get/create terminal state for a device */
TerminalState* mon_terminal_state_get(uint32_t device_no);

/* Set break strategy (MON 4B) */
int mon_set_break_strategy(uint32_t device_no, int32_t strategy,
                           const uint32_t* user_table, uint32_t max_chars);

/* Set echo strategy (MON 3B) */
int mon_set_echo_strategy(uint32_t device_no, int32_t strategy,
                          const uint32_t* user_table);

/* Check if character is a break character for the device */
bool mon_is_break_char(uint32_t device_no, uint8_t ch);

/* Check if character should be echoed for the device */
bool mon_should_echo_char(uint32_t device_no, uint8_t ch);

/* Get current break strategy for device (returns BREAK_STRAT_MAC as default) */
int32_t mon_get_break_strategy(uint32_t device_no);

/* Get current echo strategy for device (returns ECHO_STRAT_ALL as default) */
int32_t mon_get_echo_strategy(uint32_t device_no);

/* Get max chars setting for device */
uint32_t mon_get_max_chars(uint32_t device_no);

/* BitTable128 helpers */
void bit_table_clear(BitTable128* table);
void bit_table_set_bit(BitTable128* table, uint8_t bit_num);
void bit_table_clear_bit(BitTable128* table, uint8_t bit_num);
bool bit_table_test_bit(const BitTable128* table, uint8_t bit_num);
void bit_table_from_words(BitTable128* table, const uint32_t* words, int word_count);

/* Predefined break tables */
void mon_get_control_break_table(BitTable128* table);  /* Strategy 1 */
void mon_get_mac_break_table(BitTable128* table);      /* Strategy 2 */

/* Predefined echo tables */
void mon_get_no_control_echo_table(BitTable128* table);  /* Strategy 1 */

/* User table management (for DVINST to update device state) */
void mon_update_user_break_table(uint32_t device_no, const uint32_t* words);
void mon_update_user_echo_table(uint32_t device_no, const uint32_t* words);
const BitTable128* mon_get_user_break_table(uint32_t device_no);
const BitTable128* mon_get_user_echo_table(uint32_t device_no);

/* 8-bit I/O mode (TerminalFunction 112) */
void mon_set_eight_bit_io(uint32_t device_no, bool enabled);
bool mon_get_eight_bit_io(uint32_t device_no);

#endif /* MON_TERMINAL_STATE_H */
