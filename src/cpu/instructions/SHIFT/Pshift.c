#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * PSHIFT instruction - SHIFT class
 *
 * Mnemonic: pshift
 * Operands: 2
 * Opcode: 0xFEB2 (176262 octal)
 *
 * Operation: Packed Decimal Shift - Align BCD decimal point between source and destination
 *
 * Description:
 * Shifts the decimal point of a packed BCD value from the source operand to match
 * the scaling factor of the destination operand. This aligns packed decimal values
 * for arithmetic operations by adjusting the magnitude of the value.
 *
 * Both operands specify string descriptors pointing to packed BCD values in memory.
 * Each descriptor is an 8-byte structure containing:
 *   - Byte address of BCD data
 *   - Length (number of BCD digits)
 *   - Scaling factor (position of decimal point)
 *   - Format flags (packed/unpacked, sign position, etc.)
 *
 * Packed BCD Format:
 * - Each byte contains two decimal digits (nibbles 0-9)
 * - High nibble = most significant digit
 * - Low nibble = least significant digit
 * - Sign nibble: 0xC or 0xF (positive), 0xD (negative)
 * - Sign position: leading or trailing (specified in descriptor)
 *
 * Scaling Factor:
 * - Signed 8-bit value (-128 to +127)
 * - Indicates position of decimal point relative to rightmost digit
 * - Example: scaling_factor = 2 means value represents XX.YY (2 decimal places)
 * - Example: scaling_factor = -1 means value represents X0 (value × 10)
 *
 * Operation Steps:
 * 1. Load string descriptor for source from first operand address
 * 2. Load string descriptor for destination from second operand address
 * 3. Read packed BCD value from source using source descriptor
 * 4. Calculate decimal point shift:
 *    shift = dest.scaling_factor - source.scaling_factor
 * 5. Adjust value magnitude by multiplying by 10^shift:
 *    - Positive shift: moves decimal point right (increases magnitude)
 *    - Negative shift: moves decimal point left (decreases magnitude)
 *    - Zero shift: no adjustment needed
 * 6. Write adjusted value to destination using destination descriptor
 * 7. Set flags based on result
 *
 * Decimal Point Alignment Examples:
 *
 * Example 1: Align 123.45 (scaling=2) to XX.X (scaling=1)
 *   Source: value=12345, scaling_factor=2 (represents 123.45)
 *   Dest:   scaling_factor=1 (expects XX.X format)
 *   Shift:  1 - 2 = -1
 *   Result: 12345 ÷ 10 = 1234 (represents 123.4)
 *
 * Example 2: Align 5.0 (scaling=1) to X.XX (scaling=2)
 *   Source: value=50, scaling_factor=1 (represents 5.0)
 *   Dest:   scaling_factor=2 (expects X.XX format)
 *   Shift:  2 - 1 = 1
 *   Result: 50 × 10 = 500 (represents 5.00)
 *
 * Example 3: Same scaling (no adjustment)
 *   Source: value=1234, scaling_factor=2 (represents 12.34)
 *   Dest:   scaling_factor=2 (expects XX.XX format)
 *   Shift:  2 - 2 = 0
 *   Result: 1234 (represents 12.34, no change)
 *
 * Flag Behavior:
 * - Z (Zero): Set if result is zero, cleared otherwise
 * - S (Sign): Set if result is negative, cleared otherwise
 * - BO (BCD Overflow): Set by nd500_write_packed_bcd_value() if result exceeds destination field width
 * - K (Invalid): Set by nd500_write_packed_bcd_value() if BCD data is malformed during write
 * - C (Carry): Unaffected
 * - O (Overflow): Unaffected
 *
 * Trap Conditions:
 * - Invalid Operation (IVO): If source or destination BCD data is malformed
 *   - Raised by nd500_load_string_descriptor() for invalid descriptors
 *   - Raised by nd500_read_packed_bcd_value() for invalid BCD nibbles
 *   - Raised by nd500_write_packed_bcd_value() for write errors
 * - Descriptor Range (DR): If descriptor addresses are invalid
 *
 * Memory Access Pattern:
 * 1. Read 8 bytes from operand[0] address (source descriptor)
 * 2. Read 8 bytes from operand[1] address (destination descriptor)
 * 3. Read N bytes from source descriptor's data address (packed BCD source)
 *    where N = (source.field_width + 1) / 2 (2 digits per byte)
 * 4. Write M bytes to destination descriptor's data address (packed BCD result)
 *    where M = (dest.field_width + 1) / 2 (2 digits per byte)
 *
 * Performance:
 * - Execution time depends on BCD value sizes and shift magnitude
 * - Typical: 20-40 CPU cycles for moderate shifts
 * - Large shifts (|shift| > 10) may take additional cycles
 *
 * Typical Usage:
 *   ; Align currency values with different precision
 *   PSHIFT  DESC_DOLLARS, DESC_CENTS   ; Convert $123.45 to 12345 cents
 *
 *   ; Prepare values for packed decimal arithmetic
 *   PSHIFT  DESC_VALUE1, DESC_ALIGNED  ; Align to common scaling
 *   PSHIFT  DESC_VALUE2, DESC_ALIGNED
 *   PADD    DESC_ALIGNED, DESC_ALIGNED  ; Now can add with same scaling
 *
 *   ; Convert between different decimal representations
 *   PSHIFT  DESC_METERS, DESC_MILLIMETERS  ; m to mm (shift by 3)
 *
 * Notes:
 * - This is a data formatting operation, not arithmetic
 * - Precision may be lost if shifting left (dividing by 10^N)
 * - Overflow may occur if result exceeds destination field width
 * - Both descriptors must specify packed BCD format
 * - Useful for aligning values before packed decimal arithmetic operations
 * - The shift is applied to the magnitude, not the BCD representation
 * - Sign is preserved during the shift operation
 *
 * Comparison with Other Instructions:
 * - PSHIFT: Aligns decimal point (changes magnitude, preserves sign)
 * - PADD/PSUB: Requires aligned operands (same scaling factor)
 * - PCOMP: Can compare values with different scaling factors directly
 *
 * Related Instructions:
 * - PADD: Packed BCD addition (requires aligned scaling)
 * - PSUB: Packed BCD subtraction (requires aligned scaling)
 * - PMUL: Packed BCD multiplication
 * - PDIV: Packed BCD division
 * - PCOMP: Packed BCD comparison
 *
 * Reference: ND-500 Reference Manual, Packed Decimal Operations
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SHIFT/Pshift.cs
 */
void nd500_instr_Pshift(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Get descriptor addresses from operands */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t dest_desc_addr = fi->operands[1].effective_address;

    /* Load string descriptor for source from first operand address */
    Nd500StringDescriptor source_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, true, false, &source_desc)) {
        /* Invalid descriptor - K flag already set by helper */
        return;
    }

    /* Load string descriptor for destination from second operand address */
    Nd500StringDescriptor dest_desc;
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, true, false, &dest_desc)) {
        /* Invalid descriptor - K flag already set by helper */
        return;
    }

    /* Read packed BCD value from source */
    int64_t source_value = nd500_read_packed_bcd_value(cpu, &source_desc);

    /* Calculate decimal point shift between source and destination */
    int shift = dest_desc.scaling_factor - source_desc.scaling_factor;

    /* Adjust value magnitude to match destination scaling */
    int64_t result_value = source_value;
    if (shift > 0) {
        /* Positive shift: move decimal point right (multiply by 10^shift) */
        /* Example: 5.0 (scale=1) → 5.00 (scale=2): 50 × 10 = 500 */
        for (int i = 0; i < shift; i++) {
            result_value *= 10;
        }
    } else if (shift < 0) {
        /* Negative shift: move decimal point left (divide by 10^|shift|) */
        /* Example: 123.45 (scale=2) → 123.4 (scale=1): 12345 ÷ 10 = 1234 */
        for (int i = 0; i < -shift; i++) {
            result_value /= 10;
        }
    }
    /* If shift == 0, no adjustment needed */

    /* Write adjusted value to destination */
    nd500_write_packed_bcd_value(cpu, &dest_desc, result_value);

    /* Set status flags based on result */
    /* Z = 1 if result is zero */
    if (result_value == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    /* S = 1 if result is negative */
    if (result_value < 0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* BO and K flags are set by nd500_write_packed_bcd_value() if overflow or invalid BCD */
    /* C and O flags are unaffected */

    /* PC will be advanced automatically by cpu_step() */
}
