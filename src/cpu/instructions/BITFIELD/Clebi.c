#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Clebi instruction - BITFIELD class
 *
 * CLEBI - Clear Bit Immediate (Clear single bit to 0 in operand)
 *
 * Mnemonic: CLEBI
 * Format: CLEBI <operand>, <bit_number>
 * Variants: 3 (BY, H, W)
 * Operands: 2 (<operand/r/w>, <bit_number/r/byte>)
 *
 * Opcodes:
 *   0xFE7D (BY CLEBI) - Byte operand (8 bits)
 *   0xFE7E (H CLEBI)  - Halfword operand (16 bits)
 *   0xFE7F (W CLEBI)  - Word operand (32 bits)
 *
 * Operation:
 *   operand = operand & ~(1 << bit_number)
 *
 * Description:
 *   Clears a single bit to 0 in the specified operand. The bit position is
 *   specified by the bit_number operand (0-indexed from LSB). The instruction
 *   performs a bitwise AND operation with a mask that has all bits set to 1
 *   except the target bit.
 *
 *   This is an atomic read-modify-write operation on the target operand.
 *   The bit_number is interpreted as an unsigned byte value (0-255), but must
 *   be within the valid range for the operand data type:
 *   - Byte (BY): 0-7 (8 bits)
 *   - Halfword (H): 0-15 (16 bits)
 *   - Word (W): 0-31 (32 bits)
 *
 *   Common use cases:
 *   - Clearing status flags in control registers
 *   - Disabling feature bits in configuration words
 *   - Resetting individual flags in bit arrays
 *   - Releasing semaphores and locks
 *   - Hardware device register manipulation
 *
 * Bit Numbering Convention:
 *   Bits are numbered from 0 (LSB, rightmost) to N-1 (MSB, leftmost):
 *
 *   Word (32-bit):  [31 30 ... 2 1 0]
 *   Halfword (16-bit): [15 14 ... 2 1 0]
 *   Byte (8-bit):   [7 6 5 4 3 2 1 0]
 *
 * Flags: Z (zero)
 *   Z = 1 (always set - per ND-500 specification)
 *   S = unaffected (not modified by CLEBI)
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
 *   - Bit numbering from 0 (LSB) to N-1 (MSB)
 *   - Three data type variants (byte, halfword, word)
 *   - Always sets Z flag to 1 (per specification)
 *   - Traps on out-of-range bit numbers
 *   - Idempotent (clearing same bit multiple times is safe)
 *   - Can be used on memory or register operands
 *
 * Common Use Cases:
 *   - Clearing device control register bits
 *   - Disabling interrupt masks
 *   - Resetting flags in status words
 *   - Clearing bits in bitmaps
 *   - Hardware register manipulation
 *   - Semaphore and lock release
 *   - Feature flag disabling
 *   - Error flag clearing
 *
 * Example Usage:
 *   ; Disable bit 5 in control register
 *   CLEBI CONTROL_REG, 5      ; Clear bit 5 to 0
 *
 *   ; Clear multiple bits (must use multiple instructions)
 *   CLEBI STATUS, 0           ; Clear bit 0
 *   CLEBI STATUS, 3           ; Clear bit 3
 *   CLEBI STATUS, 7           ; Clear bit 7
 *
 *   ; Clear bit in memory location
 *   CLEBI (I1), 2             ; Clear bit 2 in memory at address I1
 *
 *   ; Clear bit in indexed location
 *   CLEBI TABLE(I2), 4        ; Clear bit 4 in table entry
 *
 *   ; Acknowledge interrupt by clearing status bit
 *   CLEBI INT_STATUS, 6       ; Clear interrupt pending bit 6
 *
 * Related Instructions:
 *   - SETBI: Set bit immediate (set bit to 1)
 *   - GETBI: Get single bit value to register
 *   - PUTBI: Put register LSB to bit position
 *   - GETBF: Get bit field (multiple bits) to register
 *   - PUTBF: Put register value to bit field
 *
 * Comparison with Related Instructions:
 *   - CLEBI vs AND: CLEBI clears single bit, AND clears multiple bits
 *   - CLEBI vs SETBI: CLEBI sets to 0, SETBI sets to 1
 *   - CLEBI vs PUTBI: CLEBI always sets to 0, PUTBI uses register value
 *   - CLEBI vs GETBF/PUTBF: Single bit vs multi-bit field operations
 */
void nd500_instr_Clebi(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 2) {
        printf("[ERROR] CLEBI at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read current value from first operand
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    // Read bit number from second operand (always byte-sized)
    uint64_t bit_number_raw = nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_BYTE);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
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
        printf("[ERROR] CLEBI at PC=0x%08X: Bit number %u out of range for data type (max: %u)\n",
               fi->address, bit_number, max_bit - 1);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    // Clear the bit: value & ~(1 << bit_number)
    uint64_t result = value & ~(1ULL << bit_number);

    // Mask result to data type size
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    // Write result back to first operand
    nd500_write_operand_value(cpu, &fi->operands[0], masked_result, fi->data_type);

    // Update status flags: Z=1 (always, per ND-500 specification)
    cpu->ST1 |= ND500_FLAG_Z;  // Set Z (per specification)
}
