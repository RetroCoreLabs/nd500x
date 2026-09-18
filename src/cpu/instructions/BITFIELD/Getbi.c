/*
 * Getbi.c - ND-500 Getbi instruction (BITFIELD class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Getbi instruction - BITFIELD class
 *
 * GETBI - Get Bit Immediate (Extract single bit to register)
 *
 * Mnemonic: GETBI
 * Format: Rn GETBI <operand>, <bit_number>
 * Variants: 3 (BY, H, W)
 * Operands: 2 (<operand/r>, <bit_number/r/byte>)
 *
 * Opcodes:
 *   0xFCB4 (BY1 GETBI) - Byte operand, result to I1
 *   0xFCB8 (H2 GETBI)  - Halfword operand, result to I2
 *   0xFDD0 (W3 GETBI)  - Word operand, result to I3
 *
 * Operation:
 *   bit_value = (operand >> bit_number) & 1
 *   Rn = bit_value
 *
 * Description:
 *   Extracts a single bit from the specified operand and stores the bit value
 *   (0 or 1) in the target register. The bit position is specified by the
 *   bit_number operand (0-indexed from LSB).
 *
 *   This instruction reads a bit without modifying the source operand, making it
 *   ideal for testing status flags, device register bits, and control flags.
 *
 *   The bit_number is interpreted as an unsigned byte value (0-255), but must
 *   be within the valid range for the operand data type:
 *   - Byte (BY): 0-7 (8 bits)
 *   - Halfword (H): 0-15 (16 bits)
 *   - Word (W): 0-31 (32 bits)
 *
 *   The extracted bit value (0 or 1) is stored in the target register specified
 *   by the opcode encoding. The register selection varies by opcode, typically
 *   using I1, I2, I3, or I4 integer registers.
 *
 * Register Selection:
 *   The target register is encoded in the opcode and varies by data type.
 *   Typical encoding uses integer registers I1-I4 for storing the bit value.
 *
 * Bit Numbering Convention:
 *   Bits are numbered from 0 (LSB, rightmost) to N-1 (MSB, leftmost):
 *
 *   Word (32-bit):  [31 30 ... 2 1 0]
 *   Halfword (16-bit): [15 14 ... 2 1 0]
 *   Byte (8-bit):   [7 6 5 4 3 2 1 0]
 *
 * Flags: Z (zero)
 *   Z = 1 if extracted bit value is 0
 *   Z = 0 if extracted bit value is 1
 *   S = unaffected (not modified by GETBI)
 *
 * Trap conditions:
 *   - IOV (Integer Overflow) if bit_number >= data_type_bits
 *     - Byte: bit_number >= 8
 *     - Halfword: bit_number >= 16
 *     - Word: bit_number >= 32
 *   - Addressing traps for operand access
 *
 * Performance:
 *   - Typical: 2-3 cycles
 *   - Best case: 2 cycles (register/immediate operand)
 *   - Worst case: 3+ cycles (indexed addressing)
 *
 * Key Characteristics:
 *   - Non-destructive read (source operand unchanged)
 *   - Bit value stored in target register (0 or 1)
 *   - Bit numbering from 0 (LSB) to N-1 (MSB)
 *   - Three data type variants (byte, halfword, word)
 *   - Z flag reflects extracted bit value
 *   - Traps on out-of-range bit numbers
 *   - Essential for testing individual flag bits
 *   - Can test bits in memory or registers
 *
 * Common Use Cases:
 *   - Testing device status register bits
 *   - Reading interrupt enable/disable flags
 *   - Checking feature enable bits
 *   - Reading individual flags from bit arrays
 *   - Testing error/warning flags
 *   - Polling hardware ready/busy bits
 *   - Extracting individual boolean values
 *
 * Example Usage:
 *   ; Test if interrupt enabled (bit 5 in control register)
 *   I1 GETBI CONTROL_REG, 5   ; Extract bit 5 to I1
 *   ; I1 now contains 0 or 1, Z flag set accordingly
 *
 *   ; Check device ready status (bit 7)
 *   I2 GETBI DEVICE_STATUS, 7 ; Extract ready bit to I2
 *   IF=0 DEVICE_READY         ; Branch if bit was 0 (Z=1)
 *
 *   ; Test multiple flags in sequence
 *   I1 GETBI FLAGS, 0         ; Test flag 0
 *   I2 GETBI FLAGS, 1         ; Test flag 1
 *   I3 GETBI FLAGS, 2         ; Test flag 2
 *
 *   ; Test bit in indexed location
 *   I1 GETBI TABLE(I4), 3     ; Extract bit 3 from table entry
 *
 * Related Instructions:
 *   - SETBI: Set bit to 1 (unconditionally)
 *   - CLEBI: Clear bit to 0 (unconditionally)
 *   - PUTBI: Set/clear bit based on register value
 *   - GETBF: Get multi-bit field to register
 *   - PUTBF: Put register value to multi-bit field
 *
 * Comparison with Related Instructions:
 *   - GETBI vs SETBI: GETBI reads bit, SETBI writes bit
 *   - GETBI vs PUTBI: GETBI extracts to register, PUTBI inserts from register
 *   - GETBI vs GETBF: Single bit vs multi-bit field extraction
 *   - GETBI vs TEST: GETBI stores bit value, TEST only sets flags
 */
void nd500_instr_Getbi(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 2) {
        printf("[ERROR] GETBI at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Validate target register is specified
    if (fi->target_register == 0 || fi->target_register > 4) {
        printf("[ERROR] GETBI at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read value from first operand
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
        printf("[ERROR] GETBI at PC=0x%08X: Bit number %u out of range for data type (max: %u)\n",
               fi->address, bit_number, max_bit - 1);
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    // Extract the bit: (value >> bit_number) & 1
    uint32_t bit_value = (uint32_t)((value >> bit_number) & 1);

    // Write bit value (0 or 1) to target register
    nd500_write_integer_register(cpu, fi->target_register, bit_value);

    // Update status flags per ND-500 rule 4040: all data status bits not mentioned are
    // cleared. The microcode GET_BIT_1 runs ST,SAVA which sets Z from the bit AND clears S/C/O.
    // Previously only Z was touched, leaving S/C/O stale and diverging from the microword.
    if (bit_value == 0) {
        cpu->ST1 |= ND500_FLAG_Z;   // Set Z if bit value is 0
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;  // Clear Z if bit value is 1
    }
    cpu->ST1 &= ~(ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O);
}
