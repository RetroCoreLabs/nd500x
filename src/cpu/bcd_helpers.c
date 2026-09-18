/**
 * bcd_helpers.c - packed BCD helper functions
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Based on ND-500 CPU Reference Manual, Chapter 17.
 */

#include "bcd_helpers.h"
#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <string.h>

/* Power of 10 lookup table */
const uint64_t nd500_powers_of_10[19] = {
    1ULL,                    /* 10^0 */
    10ULL,                   /* 10^1 */
    100ULL,                  /* 10^2 */
    1000ULL,                 /* 10^3 */
    10000ULL,                /* 10^4 */
    100000ULL,               /* 10^5 */
    1000000ULL,              /* 10^6 */
    10000000ULL,             /* 10^7 */
    100000000ULL,            /* 10^8 */
    1000000000ULL,           /* 10^9 */
    10000000000ULL,          /* 10^10 */
    100000000000ULL,         /* 10^11 */
    1000000000000ULL,        /* 10^12 */
    10000000000000ULL,       /* 10^13 */
    100000000000000ULL,      /* 10^14 */
    1000000000000000ULL,     /* 10^15 */
    10000000000000000ULL,    /* 10^16 */
    100000000000000000ULL,   /* 10^17 */
    1000000000000000000ULL   /* 10^18 */
};

bool nd500_bcd_sign_is_negative(uint8_t nibble)
{
    /* 0x0B and 0x0D are negative */
    return (nibble == 0x0B || nibble == 0x0D);
}

uint8_t nd500_bcd_get_sign_nibble(bool negative, Nd500BcdSignRep sign_rep)
{
    if (sign_rep == BCD_SIGN_UNSIGNED) {
        return 0x0F;  /* Unsigned marker */
    }
    return negative ? 0x0D : 0x0C;
}

uint32_t nd500_bcd_bytes_needed(uint8_t field_width, Nd500BcdSignRep sign_rep)
{
    if (field_width == 0) {
        return 0;
    }

    uint32_t digit_nibbles = field_width;
    uint32_t total_nibbles;

    switch (sign_rep) {
        case BCD_SIGN_EMBEDDED_TRAILING:
        case BCD_SIGN_EMBEDDED_LEADING:
            /* Sign shares space with a digit nibble */
            total_nibbles = digit_nibbles + 1;
            break;
        case BCD_SIGN_SEPARATE_TRAILING:
        case BCD_SIGN_SEPARATE_LEADING:
            /* Sign is in a separate byte */
            total_nibbles = digit_nibbles + 2;  /* +2 nibbles = +1 byte */
            break;
        case BCD_SIGN_UNSIGNED:
        default:
            total_nibbles = digit_nibbles;
            break;
    }

    /* Round up to bytes */
    return (total_nibbles + 1) / 2;
}

Nd500BcdDescriptor nd500_load_bcd_descriptor(Nd500Cpu* cpu, uint32_t address)
{
    Nd500BcdDescriptor desc;
    memset(&desc, 0, sizeof(desc));

    /* Read 8-byte descriptor from memory using MMU-aware functions */
    uint32_t word0 = nd500_read_memory_32(cpu, address);
    uint32_t word1 = nd500_read_memory_32(cpu, address + 4);

    /* Parse control word (word0) */
    /* Bits 26-24: Sign representation (SGN) */
    desc.sign_rep = (Nd500BcdSignRep)((word0 >> 24) & 0x07);

    /* Bits 23-18: Scaling factor (SC) - 6-bit signed */
    int8_t sc = (int8_t)((word0 >> 18) & 0x3F);
    if (sc >= 32) {
        sc -= 64;  /* Convert to signed */
    }
    desc.scaling_factor = sc;

    /* Bits 17-13: Field width (FW) */
    desc.field_width = (uint8_t)((word0 >> 13) & 0x1F);

    /* Bits 12-0: Element count (N) */
    desc.element_count = word0 & 0x1FFF;

    /* Word 1: Base address */
    desc.base_address = word1;

    /* Validate descriptor */
    desc.is_valid = (desc.field_width > 0) && (desc.sign_rep <= BCD_SIGN_UNSIGNED);

    return desc;
}

Nd500BcdResult nd500_read_packed_bcd(Nd500Cpu* cpu, const Nd500BcdDescriptor* desc)
{
    Nd500BcdResult result;
    memset(&result, 0, sizeof(result));

    if (!desc->is_valid || desc->field_width == 0) {
        result.invalid_digit = true;
        return result;
    }

    uint32_t bytes_needed = nd500_bcd_bytes_needed(desc->field_width, desc->sign_rep);
    uint32_t addr = desc->base_address;

    /* Read all bytes */
    uint8_t data[20];  /* Max 31 nibbles + sign = 16 bytes max */
    if (bytes_needed > sizeof(data)) {
        bytes_needed = sizeof(data);
    }

    for (uint32_t i = 0; i < bytes_needed; i++) {
        data[i] = nd500_read_memory_8(cpu, addr + i);
    }

    /* Extract sign and digits based on sign representation */
    uint8_t sign_nibble = 0x0C;  /* Default positive */
    int digit_start_nibble = 0;
    int digit_count = desc->field_width;

    switch (desc->sign_rep) {
        case BCD_SIGN_EMBEDDED_TRAILING:
            /* Sign is in low nibble of last byte */
            sign_nibble = data[bytes_needed - 1] & 0x0F;
            /* Digits are all nibbles except the last one */
            break;

        case BCD_SIGN_SEPARATE_TRAILING:
            /* Sign is in the last byte (low nibble) */
            sign_nibble = data[bytes_needed - 1] & 0x0F;
            break;

        case BCD_SIGN_EMBEDDED_LEADING:
            /* Sign is in high nibble of first byte */
            sign_nibble = (data[0] >> 4) & 0x0F;
            digit_start_nibble = 1;  /* Skip first nibble */
            break;

        case BCD_SIGN_SEPARATE_LEADING:
            /* Sign is in first byte (low nibble) */
            sign_nibble = data[0] & 0x0F;
            digit_start_nibble = 2;  /* Skip first byte = 2 nibbles */
            break;

        case BCD_SIGN_UNSIGNED:
            sign_nibble = 0x0F;  /* Unsigned */
            break;
    }

    result.is_negative = nd500_bcd_sign_is_negative(sign_nibble);

    /* Extract digits and build value */
    int64_t value = 0;
    int nibble_index = digit_start_nibble;

    for (int i = 0; i < digit_count; i++) {
        int byte_idx = nibble_index / 2;
        int high_nibble = (nibble_index % 2) == 0;

        uint8_t nibble;
        if (high_nibble) {
            nibble = (data[byte_idx] >> 4) & 0x0F;
        } else {
            nibble = data[byte_idx] & 0x0F;
        }

        /* Check for invalid BCD digit */
        if (nibble > 9) {
            result.invalid_digit = true;
            return result;
        }

        value = value * 10 + nibble;
        nibble_index++;
    }

    result.value = value;
    result.is_zero = (value == 0);

    return result;
}

Nd500BcdResult nd500_write_packed_bcd(Nd500Cpu* cpu, const Nd500BcdDescriptor* desc,
                                       int64_t value, bool negative)
{
    Nd500BcdResult result;
    memset(&result, 0, sizeof(result));

    if (!desc->is_valid || desc->field_width == 0) {
        result.invalid_digit = true;
        return result;
    }

    /* Get absolute value */
    uint64_t abs_value = (value < 0) ? (uint64_t)(-value) : (uint64_t)value;
    if (value < 0) {
        negative = true;
    }

    /* Check if value fits in field width */
    if (!nd500_bcd_value_fits(abs_value, desc->field_width)) {
        result.overflow = true;
        /* Still write the lower digits */
    }

    uint32_t bytes_needed = nd500_bcd_bytes_needed(desc->field_width, desc->sign_rep);
    uint8_t data[20];
    memset(data, 0, sizeof(data));

    /* Extract digits from value (least significant first) */
    uint8_t digits[32];
    int digit_count = desc->field_width;
    uint64_t temp = abs_value;

    for (int i = digit_count - 1; i >= 0; i--) {
        digits[i] = temp % 10;
        temp /= 10;
    }

    /* Pack digits into bytes based on sign representation */
    int nibble_index = 0;
    int digit_start_nibble = 0;

    switch (desc->sign_rep) {
        case BCD_SIGN_EMBEDDED_LEADING:
            /* High nibble of first byte is sign */
            data[0] = (nd500_bcd_get_sign_nibble(negative, desc->sign_rep) << 4);
            digit_start_nibble = 1;
            break;

        case BCD_SIGN_SEPARATE_LEADING:
            /* First byte is sign */
            data[0] = nd500_bcd_get_sign_nibble(negative, desc->sign_rep);
            digit_start_nibble = 2;
            break;

        default:
            digit_start_nibble = 0;
            break;
    }

    /* Pack digits */
    nibble_index = digit_start_nibble;
    for (int i = 0; i < digit_count; i++) {
        int byte_idx = nibble_index / 2;
        int high_nibble = (nibble_index % 2) == 0;

        if (high_nibble) {
            data[byte_idx] |= (digits[i] << 4);
        } else {
            data[byte_idx] |= digits[i];
        }
        nibble_index++;
    }

    /* Add trailing sign if needed */
    switch (desc->sign_rep) {
        case BCD_SIGN_EMBEDDED_TRAILING:
            /* Sign ALWAYS goes in LOW nibble for embedded trailing format.
             * The high nibble of the sign byte is either:
             * - The last digit (if nibble_index is odd)
             * - Zero padding (if nibble_index is even)
             */
            {
                int sign_byte = nibble_index / 2;
                /* Always put sign in LOW nibble, never in high nibble */
                data[sign_byte] |= nd500_bcd_get_sign_nibble(negative, desc->sign_rep);
            }
            break;

        case BCD_SIGN_SEPARATE_TRAILING:
            /* Last byte is sign */
            data[bytes_needed - 1] = nd500_bcd_get_sign_nibble(negative, desc->sign_rep);
            break;

        default:
            break;
    }

    /* Write to memory using MMU-aware functions */
    uint32_t addr = desc->base_address;
    for (uint32_t i = 0; i < bytes_needed; i++) {
        nd500_write_memory_8(cpu, addr + i, data[i]);
    }

    result.value = value;
    result.is_negative = negative;
    result.is_zero = (abs_value == 0);

    return result;
}

int64_t nd500_bcd_apply_scaling(int64_t value, int8_t sc, bool* overflow)
{
    *overflow = false;

    if (sc == 0) {
        return value;
    }

    if (sc > 0) {
        /* Multiply by 10^sc */
        if (sc > 18) {
            *overflow = true;
            return value;
        }
        uint64_t multiplier = nd500_powers_of_10[sc];
        int64_t result;
        if (value >= 0) {
            if ((uint64_t)value > (uint64_t)INT64_MAX / multiplier) {
                *overflow = true;
                return value;
            }
            result = value * (int64_t)multiplier;
        } else {
            if ((uint64_t)(-value) > (uint64_t)INT64_MAX / multiplier) {
                *overflow = true;
                return value;
            }
            result = value * (int64_t)multiplier;
        }
        return result;
    } else {
        /* Divide by 10^|sc| (truncate toward zero) */
        int8_t abs_sc = -sc;
        if (abs_sc > 18) {
            return 0;  /* Complete truncation */
        }
        uint64_t divisor = nd500_powers_of_10[abs_sc];
        return value / (int64_t)divisor;
    }
}

int64_t nd500_bcd_remove_scaling(int64_t value, int8_t sc, bool* overflow)
{
    /* Opposite of apply_scaling */
    return nd500_bcd_apply_scaling(value, -sc, overflow);
}

bool nd500_bcd_value_fits(uint64_t value, uint8_t field_width)
{
    if (field_width == 0) {
        return false;
    }
    if (field_width >= 19) {
        return true;  /* Can hold any uint64 */
    }
    /* Max value for field_width digits is 10^field_width - 1 */
    return value < nd500_powers_of_10[field_width];
}
