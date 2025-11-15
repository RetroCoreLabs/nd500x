#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Putbf instruction - BITFIELD class
 *
 * PUTBF - Put Bit Field (Insert register value into multi-bit field)
 *
 * Mnemonic: PUTBF
 * Format: Rn PUTBF <operand>, <bit_number>, <field_size>
 * Variants: 3 (BY, H, W)
 * Operands: 3 (<operand/r/w>, <bit_number/r/byte>, <field_size/r/byte>)
 *
 * Opcodes:
 *   0xFDEC (BY1 PUTBF) - Byte operand, value from I1
 *   0xFDF0 (H2 PUTBF)  - Halfword operand, value from I2
 *   0xFDF4 (W3 PUTBF)  - Word operand, value from I3
 *
 * Operation:
 *   mask = (1 << field_size) - 1
 *   cleared = operand & ~(mask << bit_number)
 *   operand = cleared | ((Rn & mask) << bit_number)
 *
 * Description:
 *   Inserts the low-order bits of the target register into a multi-bit field
 *   within the specified operand. The bit field is defined by a starting bit
 *   position (bit_number) and a field size (number of bits to insert).
 *
 *   This is an atomic read-modify-write operation on the target operand. The
 *   instruction reads the register value, extracts the low-order bits matching
 *   the field size, clears the target bit field in the operand, and then
 *   inserts the register bits at the specified position.
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
 *   Only the low-order bits of the register value (up to field_size bits) are
 *   used; high-order bits are ignored. The register selection varies by opcode,
 *   typically using I1, I2, I3, or I4 integer registers.
 *
 * Register Selection:
 *   The target register is encoded in the opcode and varies by data type.
 *   Typical encoding uses integer registers I1-I4 as the source of field value.
 *
 * Bit Numbering Convention:
 *   Bits are numbered from 0 (LSB, rightmost) to N-1 (MSB, leftmost):
 *
 *   Word (32-bit):  [31 30 ... 2 1 0]
 *   Halfword (16-bit): [15 14 ... 2 1 0]
 *   Byte (8-bit):   [7 6 5 4 3 2 1 0]
 *
 *   Example insertion (bit_number=3, field_size=5, register=0x1F):
 *   Original: [... 7 6 5 4 3 2 1 0]
 *   Cleared:  [... - - - - - 2 1 0]
 *   Result:   [... 1 1 1 1 1 2 1 0] (bits [7:3] from register)
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if inserted field value (from register) is 0
 *   Z = 0 if inserted field value is non-zero
 *   S = 1 if MSB of inserted field value is 1
 *   S = 0 if MSB of inserted field value is 0
 *
 *   Note: Flags reflect the inserted value, not the entire operand result.
 *
 * Trap conditions:
 *   - IOV (Integer Overflow) if field parameters invalid:
 *     - field_size <= 0 (must insert at least 1 bit)
 *     - bit_number >= data_type_bits (start position out of range)
 *     - (bit_number + field_size) > data_type_bits (field extends beyond operand)
 *   - Addressing traps for operand access
 *
 * Performance:
 *   - Typical: 3-4 cycles (memory read-modify-write)
 *   - Best case: 3 cycles (register operand)
 *   - Worst case: 4+ cycles (indexed addressing)
 *
 * Key Characteristics:
 *   - Atomic read-modify-write operation
 *   - Inserts 1 to N bits in a single operation
 *   - Preserves all bits outside the target field
 *   - Register high-order bits ignored (only field_size bits used)
 *   - Bit numbering from 0 (LSB) to N-1 (MSB)
 *   - Three data type variants (byte, halfword, word)
 *   - Z and S flags reflect inserted value, not entire result
 *   - Traps on invalid field parameters
 *   - Essential for packing structured data
 *   - Can insert into memory or registers
 *
 * Common Use Cases:
 *   - Packing data fields (date/time components)
 *   - Writing multi-bit device control fields
 *   - Constructing bit-packed structures
 *   - Setting color components (RGB values)
 *   - Building instruction words
 *   - Setting configuration bit groups
 *   - Assembling protocol headers
 *   - Writing multi-bit hardware control flags
 *
 * Example Usage:
 *   ; Insert 4-bit value into STATUS at bit 8
 *   I1 = 0x0F                ; Value to insert (0b1111)
 *   I1 PUTBF STATUS, 8, 4    ; Insert into bits [11:8]
 *   ; STATUS bits [11:8] now contain 0xF, other bits preserved
 *
 *   ; Set color component (5 bits for red at bit 11)
 *   I2 = RED_VALUE           ; Red component (0-31)
 *   I2 PUTBF RGB_COLOR, 11, 5 ; Insert 5-bit red component
 *
 *   ; Pack multiple fields into one word
 *   I1 = FIELD_A
 *   I1 PUTBF PACKED, 0, 4    ; Insert into bits [3:0]
 *   I2 = FIELD_B
 *   I2 PUTBF PACKED, 4, 4    ; Insert into bits [7:4]
 *   I3 = FIELD_C
 *   I3 PUTBF PACKED, 8, 8    ; Insert into bits [15:8]
 *
 *   ; Build instruction word with opcode field
 *   I1 = OPCODE              ; 4-bit opcode value
 *   I1 PUTBF INSTR_WORD, 12, 4 ; Insert opcode at bit 12
 *
 *   ; Insert into indexed location
 *   I4 = CONTROL_BITS        ; 3-bit control value
 *   I4 PUTBF TABLE(I3), 5, 3 ; Insert into table entry
 *
 *   ; Update device register field
 *   I1 = MODE_VALUE          ; New mode bits
 *   I1 PUTBF DEVICE_REG, 4, 3 ; Update 3-bit mode field
 *
 * Related Instructions:
 *   - GETBF: Get multi-bit field to register
 *   - PUTBI: Put register LSB to single bit
 *   - GETBI: Get single bit to register
 *   - SETBI: Set bit to 1 (unconditionally)
 *   - CLEBI: Clear bit to 0 (unconditionally)
 *
 * Comparison with Related Instructions:
 *   - PUTBF vs GETBF: PUTBF writes field, GETBF reads field
 *   - PUTBF vs PUTBI: Multi-bit field vs single bit insertion
 *   - PUTBF vs OR: PUTBF clears then inserts, OR only sets bits
 *   - PUTBF vs AND/OR sequence: PUTBF combines clear and insert atomically
 *
 * Typical Pattern (Extract-Modify-Insert):
 *   ; Read field, modify, write back
 *   I1 GETBF STATUS, 8, 4    ; Extract current value
 *   I1 = I1 + 1               ; Modify value
 *   I1 PUTBF STATUS, 8, 4    ; Insert modified value
 */
void nd500_instr_Putbf(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 3) {
        printf("[ERROR] PUTBF at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Validate target register is specified
    if (fi->target_register == 0 || fi->target_register > 4) {
        printf("[ERROR] PUTBF at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read current value from first operand
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    // Read bit number from second operand (always byte-sized)
    uint64_t bit_number_raw = nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_BYTE);
    uint8_t bit_number = (uint8_t)(bit_number_raw & 0xFF);

    // Read field size from third operand (always byte-sized)
    uint64_t field_size_raw = nd500_read_operand_value(cpu, &fi->operands[2], ND500_DTYPE_BYTE);
    uint8_t field_size = (uint8_t)(field_size_raw & 0xFF);

    // Calculate maximum valid bit count for this data type
    uint32_t bits = 0;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            bits = 8;
            break;
        case ND500_DTYPE_HALFWORD:
            bits = 16;
            break;
        case ND500_DTYPE_WORD:
            bits = 32;
            break;
        default:
            bits = 32;
            break;
    }

    // Validate field parameters
    if (field_size == 0) {
        printf("[ERROR] PUTBF at PC=0x%08X: Field size must be > 0\n", fi->address);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    if (bit_number >= bits) {
        printf("[ERROR] PUTBF at PC=0x%08X: Bit number %u out of range for data type (max: %u)\n",
               fi->address, bit_number, bits - 1);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    if ((bit_number + field_size) > bits) {
        printf("[ERROR] PUTBF at PC=0x%08X: Field extends beyond operand (bit %u + size %u > %u bits)\n",
               fi->address, bit_number, field_size, bits);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    // Read register value (source of field bits)
    uint32_t reg_value = nd500_read_integer_register(cpu, fi->target_register);

    // Create mask for field size
    uint64_t mask = (1ULL << field_size) - 1;

    // Clear the target field in operand: value & ~(mask << bit_number)
    uint64_t cleared_value = value & ~(mask << bit_number);

    // Insert register bits into field: cleared | ((reg_value & mask) << bit_number)
    uint64_t result = cleared_value | ((reg_value & mask) << bit_number);

    // Mask result to data type size
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    // Write result back to first operand
    nd500_write_operand_value(cpu, &fi->operands[0], masked_result, fi->data_type);

    // Extract the field value that was actually inserted (for flag setting)
    uint32_t inserted_value = reg_value & (uint32_t)mask;

    // Update status flags: Z and S based on inserted field value
    if (inserted_value == 0) {
        cpu->ST1 |= ND500_FLAG_Z;   // Set Z if inserted field is 0
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;  // Clear Z if inserted field is non-zero
    }

    // S flag: MSB of inserted field value (considering field size)
    if (field_size > 0 && (inserted_value & (1U << (field_size - 1)))) {
        cpu->ST1 |= ND500_FLAG_S;   // Set S if MSB of inserted field is 1
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;  // Clear S if MSB of inserted field is 0
    }
}
