#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Getbf instruction - BITFIELD class
 *
 * GETBF - Get Bit Field (Extract multi-bit field to register)
 *
 * Mnemonic: GETBF
 * Format: Rn GETBF <operand>, <bit_number>, <field_size>
 * Variants: 3 (BY, H, W)
 * Operands: 3 (<operand/r>, <bit_number/r/byte>, <field_size/r/byte>)
 *
 * Opcodes:
 *   0xFDE0 (BY1 GETBF) - Byte operand, result to I1
 *   0xFDE4 (H2 GETBF)  - Halfword operand, result to I2
 *   0xFDE8 (W3 GETBF)  - Word operand, result to I3
 *
 * Operation:
 *   mask = (1 << field_size) - 1
 *   bit_field = (operand >> bit_number) & mask
 *   Rn = bit_field
 *
 * Description:
 *   Extracts a multi-bit field from the specified operand and stores the field
 *   value in the target register. The bit field is defined by a starting bit
 *   position (bit_number) and a field size (number of bits to extract).
 *
 *   This instruction reads a bit field without modifying the source operand,
 *   making it ideal for extracting packed data fields, status flags, and
 *   multi-bit configuration values.
 *
 *   Both bit_number and field_size are interpreted as unsigned byte values
 *   (0-255), but must satisfy these constraints for the operand data type:
 *   - field_size must be > 0 (at least 1 bit)
 *   - bit_number must be < data_type_bits
 *   - (bit_number + field_size) must be <= data_type_bits
 *
 *   Data type bit limits:
 *   - Byte (BY): 0-7 (8 bits total)
 *   - Halfword (H): 0-15 (16 bits total)
 *   - Word (W): 0-31 (32 bits total)
 *
 *   The extracted field value is right-aligned in the target register with
 *   high-order bits cleared. The register selection varies by opcode, typically
 *   using I1, I2, I3, or I4 integer registers.
 *
 * Register Selection:
 *   The target register is encoded in the opcode and varies by data type.
 *   Typical encoding uses integer registers I1-I4 for storing the field value.
 *
 * Bit Numbering Convention:
 *   Bits are numbered from 0 (LSB, rightmost) to N-1 (MSB, leftmost):
 *
 *   Word (32-bit):  [31 30 ... 2 1 0]
 *   Halfword (16-bit): [15 14 ... 2 1 0]
 *   Byte (8-bit):   [7 6 5 4 3 2 1 0]
 *
 *   Example extraction (bit_number=3, field_size=5):
 *   Source: [... 7 6 5 4 3 2 1 0]
 *   Field:      [7 6 5 4 3]
 *   Result: [0 0 0 7 6 5 4 3] (right-aligned)
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if extracted field value is 0
 *   Z = 0 if extracted field value is non-zero
 *   S = 1 if MSB of extracted field is 1
 *   S = 0 if MSB of extracted field is 0
 *
 * Trap conditions:
 *   - IOV (Integer Overflow) if field parameters invalid:
 *     - field_size <= 0 (must extract at least 1 bit)
 *     - bit_number >= data_type_bits (start position out of range)
 *     - (bit_number + field_size) > data_type_bits (field extends beyond operand)
 *   - Addressing traps for operand access
 *
 * Performance:
 *   - Typical: 3-4 cycles
 *   - Best case: 3 cycles (register operands)
 *   - Worst case: 4+ cycles (indexed addressing)
 *
 * Key Characteristics:
 *   - Non-destructive read (source operand unchanged)
 *   - Extracts 1 to N bits in a single operation
 *   - Field value right-aligned in result
 *   - Bit numbering from 0 (LSB) to N-1 (MSB)
 *   - Three data type variants (byte, halfword, word)
 *   - Z and S flags reflect extracted field value
 *   - Traps on invalid field parameters
 *   - Essential for unpacking structured data
 *   - Can extract from memory or registers
 *
 * Common Use Cases:
 *   - Extracting packed data fields (date/time components)
 *   - Reading multi-bit device status fields
 *   - Unpacking bit-packed structures
 *   - Extracting color components (RGB values)
 *   - Reading instruction opcode fields
 *   - Extracting configuration bit groups
 *   - Parsing protocol headers
 *   - Reading multi-bit hardware flags
 *
 * Example Usage:
 *   ; Extract 4-bit field starting at bit 8 from STATUS
 *   I1 GETBF STATUS, 8, 4     ; Extract bits [11:8] to I1
 *   ; I1 now contains 4-bit value, Z/S flags set accordingly
 *
 *   ; Extract color component (5 bits for red at bit 11)
 *   I2 GETBF RGB_COLOR, 11, 5 ; Extract 5-bit red component
 *
 *   ; Extract multiple fields from packed data
 *   I1 GETBF PACKED, 0, 4     ; Extract bits [3:0]
 *   I2 GETBF PACKED, 4, 4     ; Extract bits [7:4]
 *   I3 GETBF PACKED, 8, 8     ; Extract bits [15:8]
 *
 *   ; Extract opcode field from instruction word
 *   I1 GETBF INSTR_WORD, 12, 4 ; Extract 4-bit opcode at bit 12
 *
 *   ; Extract from indexed location
 *   I4 GETBF TABLE(I3), 5, 3   ; Extract 3-bit field from table entry
 *
 * Related Instructions:
 *   - PUTBF: Put register value to multi-bit field
 *   - GETBI: Get single bit to register
 *   - PUTBI: Put register LSB to single bit
 *   - SETBI: Set bit to 1 (unconditionally)
 *   - CLEBI: Clear bit to 0 (unconditionally)
 *
 * Comparison with Related Instructions:
 *   - GETBF vs GETBI: Multi-bit field vs single bit extraction
 *   - GETBF vs PUTBF: GETBF reads field, PUTBF writes field
 *   - GETBF vs AND: GETBF extracts and shifts, AND only masks
 *   - GETBF vs SHR: GETBF combines shift and mask in one operation
 */
void nd500_instr_Getbf(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 3) {
        printf("[ERROR] GETBF at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Validate target register is specified
    if (fi->target_register == 0 || fi->target_register > 4) {
        printf("[ERROR] GETBF at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read value from first operand
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    // Read bit number from second operand (always byte-sized)
    uint64_t bit_number_raw = nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_BYTE);
    uint8_t bit_number = (uint8_t)(bit_number_raw & 0xFF);

    // Read field size from third operand (always byte-sized)
    uint64_t field_size_raw = nd500_read_operand_value(cpu, &fi->operands[2], ND500_DTYPE_BYTE);
    uint8_t field_size = (uint8_t)(field_size_raw & 0xFF);

    // For register operands, use full 32-bit register width regardless of data type
    // Data type only affects result masking, not extraction range
    bool is_register = (fi->operands[0].mode == ND500_ADDR_REGISTER);
    uint32_t bits = is_register ? 32 : (fi->data_type == ND500_DTYPE_BYTE ? 8 :
                                        fi->data_type == ND500_DTYPE_HALFWORD ? 16 : 32);

    // Validate field parameters
    if (field_size == 0) {
        printf("[ERROR] GETBF at PC=0x%08X: Field size must be > 0\n", fi->address);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    if (bit_number >= bits) {
        printf("[ERROR] GETBF at PC=0x%08X: Bit number %u out of range (max: %u)\n",
               fi->address, bit_number, bits - 1);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    if ((bit_number + field_size) > bits) {
        printf("[ERROR] GETBF at PC=0x%08X: Field extends beyond operand (bit %u + size %u > %u bits)\n",
               fi->address, bit_number, field_size, bits);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    // Extract bit field: (value >> bit_number) & mask
    uint64_t mask = (1ULL << field_size) - 1;
    uint32_t bit_field = (uint32_t)((value >> bit_number) & mask);

    // Write bit field value to target register
    nd500_write_integer_register(cpu, fi->target_register, bit_field);

    // Update status flags: Z and S based on result and data_type (not field_size)
    // Matches C# SetStatusZS - S flag uses data_type MSB, not field MSB
    nd500_set_flags_zs(cpu, (uint64_t)bit_field, fi->data_type);
}
