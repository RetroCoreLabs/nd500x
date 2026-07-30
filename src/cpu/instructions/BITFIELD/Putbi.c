#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Putbi instruction - BITFIELD class
 *
 * PUTBI - Put Bit Immediate (Set/clear bit based on register value)
 *
 * Mnemonic: PUTBI
 * Format: Rn PUTBI <operand>, <bit_number>
 * Variants: 3 (BY, H, W)
 * Operands: 2 (<operand/r/w>, <bit_number/r/byte>)
 *
 * Opcodes:
 *   0xFDD4 (BY1 PUTBI) - Byte operand, bit value from I1
 *   0xFDD8 (H2 PUTBI)  - Halfword operand, bit value from I2
 *   0xFDDC (W3 PUTBI)  - Word operand, bit value from I3
 *
 * Operation:
 *   bit_value = Rn & 1
 *   if (bit_value == 1)
 *       operand = operand | (1 << bit_number)
 *   else
 *       operand = operand & ~(1 << bit_number)
 *
 * Description:
 *   Sets or clears a single bit in the specified operand based on the LSB
 *   (least significant bit) of the target register. If the register's LSB is 1,
 *   the bit is set; if 0, the bit is cleared.
 *
 *   This is an atomic read-modify-write operation on the target operand.
 *   The bit_number is interpreted as an unsigned byte value (0-255), but must
 *   be within the valid range for the operand data type:
 *   - Byte (BY): 0-7 (8 bits)
 *   - Halfword (H): 0-15 (16 bits)
 *   - Word (W): 0-31 (32 bits)
 *
 *   Unlike SETBI (always sets to 1) and CLEBI (always clears to 0), PUTBI
 *   uses the value in the target register to determine whether to set or clear
 *   the bit. This makes it ideal for copying bit values between locations or
 *   applying computed boolean values.
 *
 * Register Selection:
 *   The target register is encoded in the opcode and contains the bit value
 *   to write. Only the LSB of the register is used; all other bits are ignored.
 *   Typical encoding uses integer registers I1-I4.
 *
 * Bit Numbering Convention:
 *   Bits are numbered from 0 (LSB, rightmost) to N-1 (MSB, leftmost):
 *
 *   Word (32-bit):  [31 30 ... 2 1 0]
 *   Halfword (16-bit): [15 14 ... 2 1 0]
 *   Byte (8-bit):   [7 6 5 4 3 2 1 0]
 *
 * Flags: Z (zero)
 *   Z = 1 if register LSB is 0 (bit was cleared)
 *   Z = 0 if register LSB is 1 (bit was set)
 *   S = unaffected (not modified by PUTBI)
 *
 * Trap conditions:
 *   - IOV (Integer Overflow) if bit_number >= data_type_bits
 *     - Byte: bit_number >= 8
 *     - Halfword: bit_number >= 16
 *     - Word: bit_number >= 32
 *   - Addressing traps for operand access
 *
 * Performance:
 *   - Typical: 2-3 cycles (memory read-modify-write)
 *   - Best case: 2 cycles (register operand)
 *   - Worst case: 3+ cycles (indexed addressing)
 *
 * Key Characteristics:
 *   - Atomic read-modify-write operation
 *   - Bit value determined by register LSB (0 or 1)
 *   - All register bits except LSB are ignored
 *   - Bit numbering from 0 (LSB) to N-1 (MSB)
 *   - Three data type variants (byte, halfword, word)
 *   - Z flag reflects bit value written
 *   - Traps on out-of-range bit numbers
 *   - Flexible bit manipulation (set or clear based on data)
 *   - Can operate on memory or register operands
 *
 * Common Use Cases:
 *   - Copying bit values between locations
 *   - Applying computed boolean results to flags
 *   - Conditional bit setting/clearing
 *   - Implementing software-controlled status bits
 *   - Writing device control register bits
 *   - Updating interrupt enable/disable flags
 *   - Setting bits based on runtime conditions
 *
 * Example Usage:
 *   ; Copy bit 5 from STATUS to bit 3 in CONTROL
 *   I1 GETBI STATUS, 5        ; Extract bit 5 to I1
 *   I1 PUTBI CONTROL, 3       ; Set/clear bit 3 based on I1
 *
 *   ; Set bit based on comparison result
 *   COMP VALUE, THRESHOLD     ; Compare values
 *   I1 = 0                    ; Clear I1
 *   IF> I1 = 1                ; Set I1 if greater
 *   I1 PUTBI FLAGS, 7         ; Update flag bit 7
 *
 *   ; Toggle bit (with additional logic)
 *   I1 GETBI STATUS, 2        ; Read current bit value
 *   I1 XOR 1                  ; Invert bit
 *   I1 PUTBI STATUS, 2        ; Write back inverted value
 *
 *   ; Apply calculated bit value
 *   ; (I1 contains 0 or 1 from some calculation)
 *   I1 PUTBI DEVICE_REG, 4    ; Set/clear bit 4 in device register
 *
 * Related Instructions:
 *   - SETBI: Set bit to 1 (unconditionally)
 *   - CLEBI: Clear bit to 0 (unconditionally)
 *   - GETBI: Extract bit value to register
 *   - GETBF: Get multi-bit field to register
 *   - PUTBF: Put register value to multi-bit field
 *
 * Comparison with Related Instructions:
 *   - PUTBI vs SETBI: PUTBI uses register value, SETBI always sets to 1
 *   - PUTBI vs CLEBI: PUTBI uses register value, CLEBI always clears to 0
 *   - PUTBI vs GETBI: PUTBI writes bit, GETBI reads bit
 *   - PUTBI vs PUTBF: Single bit vs multi-bit field insertion
 */
void nd500_instr_Putbi(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 2) {
        printf("[ERROR] PUTBI at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Validate target register is specified
    if (fi->target_register == 0 || fi->target_register > 4) {
        printf("[ERROR] PUTBI at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read current value from first operand
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    // Read bit number from second operand (always byte-sized)
    uint64_t bit_number_raw = nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_BYTE);
    uint8_t bit_number = (uint8_t)(bit_number_raw & 0xFF);

    // Calculate maximum valid bit number for this data type
    uint32_t max_bit = 0;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            max_bit = 8;
            break;
        case ND500_DTYPE_HALFWORD:
            max_bit = 16;
            break;
        case ND500_DTYPE_WORD:
            max_bit = 32;
            break;
        default:
            max_bit = 32;
            break;
    }

    // Check if bit number is within valid range
    if (bit_number >= max_bit) {
        printf("[ERROR] PUTBI at PC=0x%08X: Bit number %u out of range for data type (max: %u)\n",
               fi->address, bit_number, max_bit - 1);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    // Read bit value from target register LSB (0 or 1)
    uint32_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint32_t bit_value = reg_value & 1;  // Extract LSB only

    // Set or clear the bit based on register LSB
    uint64_t result;
    if (bit_value == 1) {
        // Set bit: value | (1 << bit_number)
        result = value | (1ULL << bit_number);
    } else {
        // Clear bit: value & ~(1 << bit_number)
        result = value & ~(1ULL << bit_number);
    }

    // Mask result to data type size
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    // Write result back to first operand
    nd500_write_operand_value(cpu, &fi->operands[0], masked_result, fi->data_type);

    // Update status flags per ND-500 rule 4040: all data status bits not mentioned are
    // cleared. The microcode PUT_BIT_1 runs ST,SAVA which sets Z from the bit AND clears S/C/O.
    // Previously only Z was touched, leaving S/C/O stale and diverging from the microword.
    if (bit_value == 0) {
        cpu->ST1 |= ND500_FLAG_Z;   // Set Z if bit value is 0
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;  // Clear Z if bit value is 1
    }
    cpu->ST1 &= ~(ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O);
}
