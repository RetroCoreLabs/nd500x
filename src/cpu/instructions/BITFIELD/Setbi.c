/*
 * Setbi.c - ND-500 Setbi instruction (BITFIELD class)
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

/**
 * Setbi instruction - BITFIELD class
 *
 * SETBI - Set Bit Immediate (Set single bit to 1 in operand)
 *
 * Mnemonic: SETBI
 * Format: SETBI <operand>, <bit_number>
 * Variants: 3 (BY, H, W)
 * Operands: 2 (<operand/r/w>, <bit_number/r/byte>)
 *
 * Opcodes:
 *   0xFE80 (BY SETBI) - Byte operand (8 bits)
 *   0xFE81 (H SETBI)  - Halfword operand (16 bits)
 *   0xFE82 (W SETBI)  - Word operand (32 bits)
 *
 * Operation:
 *   operand = operand | (1 << bit_number)
 *
 * Description:
 *   Sets a single bit to 1 in the specified operand. The bit position is
 *   specified by the bit_number operand (0-indexed from LSB). The instruction
 *   performs a bitwise OR operation with a mask containing only the target bit.
 *
 *   This is an atomic read-modify-write operation on the target operand.
 *   The bit_number is interpreted as an unsigned byte value (0-255), but must
 *   be within the valid range for the operand data type:
 *   - Byte (BY): 0-7 (8 bits)
 *   - Halfword (H): 0-15 (16 bits)
 *   - Word (W): 0-31 (32 bits)
 *
 *   Common use cases:
 *   - Setting status flags in control registers
 *   - Enabling feature bits in configuration words
 *   - Setting individual flags in bit arrays
 *   - Implementing semaphores and locks
 *   - Hardware device register manipulation
 *
 * Bit Numbering Convention:
 *   Bits are numbered from 0 (LSB, rightmost) to N-1 (MSB, leftmost):
 *
 *   Word (32-bit):  [31 30 ... 2 1 0]
 *   Halfword (16-bit): [15 14 ... 2 1 0]
 *   Byte (8-bit):   [7 6 5 4 3 2 1 0]
 *
 * Flags: Z (zero), S (sign)
 *   Z = 0 (always cleared - result cannot be zero after setting a bit)
 *   S = 0 (always cleared - sign not relevant for bit operations)
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
 *   - Always clears Z and S flags
 *   - Traps on out-of-range bit numbers
 *   - Idempotent (setting same bit multiple times is safe)
 *   - Can be used on memory or register operands
 *
 * Common Use Cases:
 *   - Setting device control register bits
 *   - Enabling interrupt masks
 *   - Setting flags in status words
 *   - Implementing bit arrays and bitmaps
 *   - Hardware register manipulation
 *   - Semaphore and lock implementation
 *   - Feature flag enabling
 *
 * Example Usage:
 *   ; Enable bit 5 in control register
 *   SETBI CONTROL_REG, 5      ; Set bit 5 to 1
 *
 *   ; Set multiple bits (must use multiple instructions)
 *   SETBI STATUS, 0           ; Enable bit 0
 *   SETBI STATUS, 3           ; Enable bit 3
 *   SETBI STATUS, 7           ; Enable bit 7
 *
 *   ; Set bit in memory location
 *   SETBI (I1), 2             ; Set bit 2 in memory at address I1
 *
 *   ; Set bit in indexed location
 *   SETBI TABLE(I2), 4        ; Set bit 4 in table entry
 *
 * Related Instructions:
 *   - CLEBI: Clear bit immediate (set bit to 0)
 *   - GETBI: Get single bit value to register
 *   - PUTBI: Put register LSB to bit position
 *   - GETBF: Get bit field (multiple bits) to register
 *   - PUTBF: Put register value to bit field
 *
 * Comparison with Related Instructions:
 *   - SETBI vs OR: SETBI sets single bit, OR sets multiple bits
 *   - SETBI vs CLEBI: SETBI sets to 1, CLEBI sets to 0
 *   - SETBI vs PUTBI: SETBI always sets to 1, PUTBI uses register value
 *   - SETBI vs GETBF/PUTBF: Single bit vs multi-bit field operations
 */
void nd500_instr_Setbi(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 2) {
        printf("[ERROR] SETBI at PC=0x%08X: Expected 2 operands, got %u\n",
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
        printf("[ERROR] SETBI at PC=0x%08X: Bit number %u out of range for data type (max: %u)\n",
               fi->address, bit_number, max_bit - 1);
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    // Set the bit: value | (1 << bit_number)
    uint64_t result = value | (1ULL << bit_number);

    // Mask result to data type size
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    // Write result back to first operand
    nd500_write_operand_value(cpu, &fi->operands[0], masked_result, fi->data_type);

    // Update status flags: the manual says "All cleared" (all data status bits: Z C S O)
    cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_C | ND500_FLAG_S | ND500_FLAG_O);
}
