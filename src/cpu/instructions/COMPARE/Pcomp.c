/*
 * Pcomp.c - ND-500 PCOMP instruction (COMPARE class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * PCOMP instruction - COMPARE class
 *
 * Mnemonic: pcomp
 * Operands: 2
 * Opcode: 0xFEB3 (176263 octal)
 *
 * Operation: Compare packed BCD strings
 *
 * Description:
 * Compares two packed BCD values by computing (A - B) and setting status flags
 * based on the result. The actual difference is discarded; only the comparison
 * result is reflected in the status flags.
 *
 * Both operands specify string descriptors pointing to packed BCD values in memory.
 * Each descriptor is an 8-byte structure in memory containing:
 *   - Byte address of BCD data
 *   - Length (number of BCD digits)
 *   - Format flags (packed/unpacked, sign position, etc.)
 *
 * Packed BCD Format:
 * - Each byte contains two decimal digits (nibbles 0-9)
 * - High nibble = most significant digit
 * - Low nibble = least significant digit
 * - Sign nibble: 0xC or 0xF (positive), 0xD (negative)
 * - Sign position: leading or trailing (specified in descriptor)
 *
 * Operation Steps:
 * 1. Load string descriptor A from first operand address
 * 2. Load string descriptor B from second operand address
 * 3. Read packed BCD value A from memory using descriptor A
 * 4. Read packed BCD value B from memory using descriptor B
 * 5. Compute difference: diff = A - B
 * 6. Set flags based on difference:
 *    - Z = 1 if diff == 0 (values are equal)
 *    - S = 1 if diff < 0 (A is less than B)
 *    - K = 1 if invalid BCD data encountered (set by helper)
 * 7. Discard the computed difference (comparison only)
 *
 * Flag Behavior:
 * - Z (Zero): Set if A == B, cleared otherwise
 * - S (Sign): Set if A < B, cleared otherwise
 * - K (Invalid): Set if BCD data contains invalid nibbles (A-F in data nibbles)
 * - C (Carry): Unaffected
 * - O (Overflow): Unaffected
 *
 * Comparison Results:
 *   A > B:  Z=0, S=0 (positive difference)
 *   A == B: Z=1, S=0 (zero difference)
 *   A < B:  Z=0, S=1 (negative difference)
 *
 * Invalid BCD Detection:
 * The K flag is set by nd500_read_packed_bcd_value() if:
 * - Data nibbles contain values > 9 (A-F)
 * - Sign nibble is invalid (not C, D, or F)
 * - Descriptor specifies invalid length or address
 *
 * BCD Value Range:
 * - Maximum digits: up to 31 (limited by descriptor length field)
 * - Represented as C double (+/-1.7e308 range, 15-17 decimal digits precision)
 * - Values exceeding double precision may lose accuracy in comparison
 *
 * Trap Conditions:
 * - Invalid Operation (IVO): If BCD data is malformed
 *   - Raised by nd500_read_packed_bcd_value() helper
 *   - K flag set to indicate invalid data detected
 *
 * Memory Access Pattern:
 * 1. Read 8 bytes from operand[0] address (descriptor A)
 * 2. Read 8 bytes from operand[1] address (descriptor B)
 * 3. Read N bytes from descriptor A's data address (BCD value A)
 * 4. Read M bytes from descriptor B's data address (BCD value B)
 *    where N and M are derived from descriptor length fields
 *
 * Performance:
 * - Execution time depends on BCD string lengths
 * - Typical: 10-30 CPU cycles for short values (1-4 digits)
 * - Longer values: additional cycles per digit pair
 *
 * Typical Usage:
 *   ; Compare account balances (packed BCD)
 *   PCOMP  DESC_BALANCE_A, DESC_BALANCE_B
 *   JZ     EQUAL_BALANCE       ; Branch if A == B
 *   JS     A_LESS_THAN_B       ; Branch if A < B
 *   ; Fall through: A > B
 *
 *   ; Validate minimum value
 *   PCOMP  DESC_VALUE, DESC_MINIMUM
 *   JS     VALUE_TOO_LOW       ; Branch if value < minimum
 *
 * Notes:
 * - This is a comparison-only operation; no result is stored
 * - Use PSUB to compute and store the actual difference
 * - BCD arithmetic is decimal-based (no binary rounding)
 * - Comparison respects decimal precision (e.g., 1.00 == 1.0)
 * - Both operands must be valid string descriptors
 * - Invalid BCD data sets K flag but comparison may still set Z/S flags
 * - Useful for financial/accounting applications requiring exact decimal comparison
 *
 * Related Instructions:
 * - SCOMP: Compare unpacked string (ASCII digits)
 * - PSUB: Packed BCD subtraction (stores result)
 * - PADD: Packed BCD addition
 * - PMUL: Packed BCD multiplication
 *
 * Reference: ND-500 Reference Manual ND-05.009.4 EN, Page 239, Section 13.12
 */
void nd500_instr_Pcomp(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Get descriptor addresses from operands */
    uint32_t desc_addr_a = fi->operands[0].effective_address;
    uint32_t desc_addr_b = fi->operands[1].effective_address;

    /* Load string descriptor A from first operand address */
    Nd500StringDescriptor desc_a;
    if (!nd500_load_string_descriptor(cpu, desc_addr_a, true, false, &desc_a)) {
        /* Invalid descriptor - K flag already set by helper */
        return;
    }

    /* Load string descriptor B from second operand address */
    Nd500StringDescriptor desc_b;
    if (!nd500_load_string_descriptor(cpu, desc_addr_b, true, false, &desc_b)) {
        /* Invalid descriptor - K flag already set by helper */
        return;
    }

    /* Read packed BCD value A from memory */
    double value_a = nd500_read_packed_bcd_value(cpu, &desc_a);

    /* Read packed BCD value B from memory */
    double value_b = nd500_read_packed_bcd_value(cpu, &desc_b);

    /* Compute difference: A - B */
    double diff = value_a - value_b;

    /* Set flags based on comparison result */
    /* Z = 1 if A == B (difference is zero) */
    if (fabs(diff) < 1e-15) {  /* Use epsilon for floating-point comparison */
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    /* S = 1 if A < B (difference is negative) */
    if (diff < 0.0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* K flag is set by nd500_read_packed_bcd_value() if invalid BCD data */
    /* C and O flags are unaffected */

    /* Difference is discarded - this is comparison only */
    /* PC will be advanced automatically by cpu_step() */
}
