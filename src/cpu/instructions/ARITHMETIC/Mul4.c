/*
 * Mul4.c - ND-500 Mul4 instruction (ARITHMETIC class)
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
 * Mul4 instruction - ARITHMETIC class
 *
 * Multiply with Overflow to Register: <a> * <b> -> <c> (lower), Rn (upper)
 *
 * Variants: 12
 * Mnemonics: mul4
 * Operands: 3 (<a/r/t>, <b/r/t>, <c/w/t>)
 *
 * Opcodes:
 *   0xFC20 (BYn MUL4) byte multiply (n=1)
 *   0xFC21 (BYn MUL4) byte multiply (n=2)
 *   0xFC22 (BYn MUL4) byte multiply (n=3)
 *   0xFC23 (BYn MUL4) byte multiply (n=4)
 *   0xFC24 (Hn MUL4)  halfword multiply (n=1)
 *   0xFC25 (Hn MUL4)  halfword multiply (n=2)
 *   0xFC26 (Hn MUL4)  halfword multiply (n=3)
 *   0xFC27 (Hn MUL4)  halfword multiply (n=4)
 *   0xFC28 (Wn MUL4)  word multiply (n=1)
 *   0xFC29 (Wn MUL4)  word multiply (n=2)
 *   0xFC2A (Wn MUL4)  word multiply (n=3)
 *   0xFC2B (Wn MUL4)  word multiply (n=4)
 *
 * Operation: <c> = lower_half(<a> * <b>), Rn = upper_half(<a> * <b>)
 *
 * Description:
 *   Multiplies the <a> operand by the <b> operand and stores the lower half
 *   of the product in the <c> operand (destination). The upper half of the
 *   double-length result is stored in the specified register Rn.
 *
 *   This instruction provides access to the full double-length product of a
 *   multiplication, which is essential for:
 *     - Multi-precision arithmetic
 *     - Overflow detection
 *     - Cryptographic operations
 *     - Bignum calculations
 *
 *   Integer overflow occurs if the upper half is not equal to the sign
 *   extension of the lower half.
 *
 *   Register selection is encoded in opcode bits 1-0:
 *   - 00 -> register 1 (I1)
 *   - 01 -> register 2 (I2)
 *   - 10 -> register 3 (I3)
 *   - 11 -> register 4 (I4)
 *
 *   Data Types: BY (byte), H (halfword), W (word) - integer only
 *   NO floating-point support
 *
 * Flags: Z (zero), S (sign), O (overflow)
 *   Z = 1 if lower part of product is zero
 *   S = 1 if sign bit of lower part is set
 *   O = 1 if overflow (upper half != sign extension of lower)
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *
 * Key Characteristics:
 *   - Four-operand multiplication (includes implicit register Rn)
 *   - Full double-length product access (no precision loss)
 *   - Integer-only (no float/double support)
 *   - Essential for multi-precision arithmetic
 *   - Upper half enables overflow detection
 *   - 12 variants (3 types x 4 registers)
 *
 * Performance Notes:
 *   - Slightly slower than MUL3 (extra register store)
 *   - Typical: 6-9 cycles depending on addressing modes
 *   - Best case: 6 cycles (register to register)
 *   - Worst case: 9+ cycles (memory to memory)
 *
 * Reference: ND-500 Reference Manual, section 11.13 (Multiply with overflow)
 *            docs/instructions/asm/mul4.md
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Mul4.cs
 */
void nd500_instr_Mul4(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operands and target register
    if (!nd500_validate_operand_count(cpu, fi, 3, INSTR_MUL4)) return;
    if (!nd500_validate_target_register(cpu, fi, INSTR_MUL4)) return;
    /* MUL4 has only BY/H/W variants (prefixes_mask 0x0E) - no float check needed */

    // Read operands
    uint64_t operandA = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    uint64_t operandB = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    // Sign-extend operands based on data type (signed multiplication)
    int64_t signedA = 0;
    int64_t signedB = 0;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            signedA = (int8_t)(operandA & 0xFF);
            signedB = (int8_t)(operandB & 0xFF);
            break;
        case ND500_DTYPE_HALFWORD:
            signedA = (int16_t)(operandA & 0xFFFF);
            signedB = (int16_t)(operandB & 0xFFFF);
            break;
        case ND500_DTYPE_WORD:
            signedA = (int32_t)(operandA & 0xFFFFFFFF);
            signedB = (int32_t)(operandB & 0xFFFFFFFF);
            break;
        default:
            signedA = (int64_t)operandA;
            signedB = (int64_t)operandB;
            break;
    }

    // Perform full-width signed multiplication
    int64_t fullProduct = signedA * signedB;

    // Extract lower and upper parts based on data type
    uint32_t lowerPart = 0;
    uint32_t upperPart = 0;
    bool overflow = false;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            lowerPart = (uint32_t)(fullProduct & 0xFF);
            upperPart = (uint32_t)((fullProduct >> 8) & 0xFF);
            // Overflow if product doesn't fit in signed byte
            overflow = (fullProduct < INT8_MIN || fullProduct > INT8_MAX);
            break;
        case ND500_DTYPE_HALFWORD:
            lowerPart = (uint32_t)(fullProduct & 0xFFFF);
            upperPart = (uint32_t)((fullProduct >> 16) & 0xFFFF);
            // Overflow if product doesn't fit in signed halfword
            overflow = (fullProduct < INT16_MIN || fullProduct > INT16_MAX);
            break;
        case ND500_DTYPE_WORD:
            lowerPart = (uint32_t)(fullProduct & 0xFFFFFFFF);
            upperPart = (uint32_t)((fullProduct >> 32) & 0xFFFFFFFF);
            // Overflow if product doesn't fit in signed word
            overflow = (fullProduct < INT32_MIN || fullProduct > INT32_MAX);
            break;
        default:
            lowerPart = (uint32_t)(fullProduct & 0xFFFFFFFF);
            upperPart = (uint32_t)((fullProduct >> 32) & 0xFFFFFFFF);
            overflow = false;
            break;
    }

    // Write lower part to destination operand (third operand)
    nd500_write_operand_value(cpu, &fi->operands[2], lowerPart, fi->data_type);

    // Write upper part to specified integer register
    nd500_write_integer_register(cpu, fi->target_register, upperPart);

    // Update status flags based on lower part of product
    if (lowerPart == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    // Set sign bit based on lower part
    bool sign_bit = nd500_is_negative(lowerPart, fi->data_type);
    if (sign_bit) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }

    /* Integer overflow raises O (bit 9), not IVO (bit 11). This comment used
     * to say "integer overflow uses invalid operation trap", which described
     * the bug fixed in 694bac2 rather than the manual: ND-05.009.4 6.5.3.1
     * reserves IVO for a different condition. Note the O FLAG set above is the
     * SAME bit 9 - there is one status register, so the flag and the trap
     * condition are one bit. */
    if (overflow) {
        trap_integer_overflow(cpu, fi->address);
    }
}
