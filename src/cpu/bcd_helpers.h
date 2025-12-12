/**
 * ND-500 Packed BCD Helper Functions
 *
 * This module provides helper functions for reading and writing
 * packed BCD (Binary-Coded Decimal) values using the ND-500
 * string descriptor format.
 *
 * Based on ND-500 CPU Reference Manual, Chapter 17.
 */

#ifndef ND500_BCD_HELPERS_H
#define ND500_BCD_HELPERS_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declaration */
typedef struct Nd500Cpu Nd500Cpu;

/**
 * Sign representation types (SGN field, bits 26-24 of descriptor)
 */
typedef enum {
    BCD_SIGN_EMBEDDED_TRAILING = 0,  /* Sign in low nibble of last digit byte */
    BCD_SIGN_SEPARATE_TRAILING = 1,  /* Sign in separate byte after digits */
    BCD_SIGN_EMBEDDED_LEADING = 2,   /* Sign in high nibble of first digit byte */
    BCD_SIGN_SEPARATE_LEADING = 3,   /* Sign in separate byte before digits */
    BCD_SIGN_UNSIGNED = 4            /* No sign nibble (always positive) */
} Nd500BcdSignRep;

/**
 * Packed BCD String Descriptor
 *
 * Parsed from the 8-byte memory descriptor format.
 */
typedef struct {
    uint32_t base_address;      /* Address of BCD data */
    uint32_t element_count;     /* Number of elements (N) - bits 12-0 */
    Nd500BcdSignRep sign_rep;   /* Sign representation (SGN) - bits 26-24 */
    int8_t scaling_factor;      /* Decimal point position (SC) - bits 23-18, signed -32 to +31 */
    uint8_t field_width;        /* Number of digit nibbles (FW) - bits 17-13, 0-31 */
    bool is_valid;              /* True if descriptor is valid */
} Nd500BcdDescriptor;

/**
 * BCD operation result
 */
typedef struct {
    int64_t value;              /* The integer value (before scaling) */
    bool is_negative;           /* Sign of the value */
    bool overflow;              /* True if overflow occurred */
    bool invalid_digit;         /* True if invalid BCD digit detected */
    bool is_zero;               /* True if value is zero */
} Nd500BcdResult;

/**
 * Load a BCD descriptor from memory
 *
 * @param cpu       CPU instance for memory access
 * @param address   Address of the 8-byte descriptor
 * @return          Parsed descriptor structure
 */
Nd500BcdDescriptor nd500_load_bcd_descriptor(Nd500Cpu* cpu, uint32_t address);

/**
 * Read a packed BCD value from memory
 *
 * Reads the BCD data using the descriptor and converts to an integer value.
 * The scaling factor is NOT applied - caller must handle scaling.
 *
 * @param cpu       CPU instance for memory access
 * @param desc      Parsed BCD descriptor
 * @return          Result containing value, sign, and status flags
 */
Nd500BcdResult nd500_read_packed_bcd(Nd500Cpu* cpu, const Nd500BcdDescriptor* desc);

/**
 * Write a packed BCD value to memory
 *
 * Converts the integer value to packed BCD format and writes to memory.
 * The scaling factor is NOT applied - caller must pre-scale the value.
 *
 * @param cpu       CPU instance for memory access
 * @param desc      Parsed BCD descriptor
 * @param value     Integer value to write (magnitude)
 * @param negative  True if value is negative
 * @return          Result with overflow status
 */
Nd500BcdResult nd500_write_packed_bcd(Nd500Cpu* cpu, const Nd500BcdDescriptor* desc,
                                       int64_t value, bool negative);

/**
 * Check if a sign nibble indicates negative
 *
 * @param nibble    The sign nibble value (0x00-0x0F)
 * @return          True if negative (0x0B or 0x0D)
 */
bool nd500_bcd_sign_is_negative(uint8_t nibble);

/**
 * Get the sign nibble for a given sign and representation
 *
 * @param negative  True if value is negative
 * @param sign_rep  Sign representation type
 * @return          Sign nibble value
 */
uint8_t nd500_bcd_get_sign_nibble(bool negative, Nd500BcdSignRep sign_rep);

/**
 * Calculate bytes needed for a BCD field
 *
 * @param field_width   Number of digit nibbles
 * @param sign_rep      Sign representation type
 * @return              Number of bytes needed
 */
uint32_t nd500_bcd_bytes_needed(uint8_t field_width, Nd500BcdSignRep sign_rep);

/**
 * Apply scaling factor to convert BCD value to integer
 *
 * For positive SC: multiply by 10^SC
 * For negative SC: divide by 10^|SC| (truncate)
 *
 * @param value     The raw BCD value (unscaled)
 * @param sc        Scaling factor (-32 to +31)
 * @param overflow  Set to true if overflow occurs
 * @return          Scaled value
 */
int64_t nd500_bcd_apply_scaling(int64_t value, int8_t sc, bool* overflow);

/**
 * Remove scaling factor from integer for BCD storage
 *
 * For positive SC: divide by 10^SC
 * For negative SC: multiply by 10^|SC|
 *
 * @param value     The integer value
 * @param sc        Scaling factor (-32 to +31)
 * @param overflow  Set to true if overflow occurs
 * @return          Value adjusted for BCD storage
 */
int64_t nd500_bcd_remove_scaling(int64_t value, int8_t sc, bool* overflow);

/**
 * Check if a value fits in the given field width
 *
 * @param value         Absolute value to check
 * @param field_width   Number of digit nibbles available
 * @return              True if value fits
 */
bool nd500_bcd_value_fits(uint64_t value, uint8_t field_width);

/**
 * Power of 10 lookup (10^n for n=0..18)
 */
extern const uint64_t nd500_powers_of_10[19];

#endif /* ND500_BCD_HELPERS_H */
