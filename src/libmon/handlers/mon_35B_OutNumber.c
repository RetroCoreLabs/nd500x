/*
 * MON 35B [IOUT/OutNumber]
 *
 * Writes a number to the user's terminal.
 * The number can be output as binary, octal, decimal, or hexadecimal.
 *
 * The number may be in the range -32768 to 32767.
 *
 * Parameters:
 *   [I] Format (INTEGER): Output format:
 *       2 = binary (bit pattern)
 *       8 = octal
 *       10 = decimal
 *       16 = hexadecimal
 *   [I] Number (INTEGER): The number to be written (-32768 to 32767).
 *
 * Returns:
 *   No error return
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"
#include <stdio.h>
#include <string.h>

/* Helper to write a string to console */
static void write_string_to_console(const char* text) {
    ConsoleIO* console = mon_file_table_get_console();

    for (int i = 0; text[i] != '\0'; i++) {
        if (console && console->write_char) {
            console->write_char(console->context, (uint8_t)text[i]);
        } else {
            putchar(text[i]);
        }
    }

    if (!console || !console->write_char) {
        fflush(stdout);
    }
}

/* Helper to convert unsigned to binary string (16 bits) */
static void to_binary_string(uint16_t value, char* buf) {
    for (int i = 15; i >= 0; i--) {
        buf[15 - i] = ((value >> i) & 1) ? '1' : '0';
    }
    buf[16] = '\0';
}

/* Helper to convert integer to octal string with sign */
static void to_octal_string(int16_t value, char* buf) {
    if (value < 0) {
        buf[0] = '-';
        sprintf(buf + 1, "%o", (unsigned int)(-value));
    } else {
        sprintf(buf, "%o", (unsigned int)value);
    }
}

/* Helper to convert integer to hex string with sign */
static void to_hex_string(int16_t value, char* buf) {
    if (value < 0) {
        buf[0] = '-';
        sprintf(buf + 1, "%X", (unsigned int)(-value));
    } else {
        sprintf(buf, "%X", (unsigned int)value);
    }
}

MonResult mon_35B_OutNumber(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_35B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t format = mon_read_param_word(ctx, 0);
    uint32_t raw_number = mon_read_param_word(ctx, 1);
    int16_t number = (int16_t)(raw_number & 0xFFFF);

    MON_LOG_IN_WORD(ctx, 0, "Format");
    MON_LOG_IN_WORD(ctx, 1, "Number");

    mon_log(MON_LOG_DEBUG, MON_ID_35B ": IN: Format=%o, Number=%d",
            format, number);

    /* Buffer for output string */
    char output[32];

    switch (format) {
        case 2:
            /* Binary - show 16-bit pattern */
            to_binary_string((uint16_t)number, output);
            break;
        case 8:
            /* Octal */
            to_octal_string(number, output);
            break;
        case 16:
            /* Hexadecimal */
            to_hex_string(number, output);
            break;
        case 10:
        default:
            /* Decimal (default) */
            sprintf(output, "%d", number);
            break;
    }

    /* Output to console */
    write_string_to_console(output);

    mon_log(MON_LOG_DEBUG, MON_ID_35B ": OUT: Output=\"%s\"", output);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
