/*
 * IfUnsignedGreaterGo.c - ND-500 IfUnsignedGreaterGo instruction (BRANCH class)
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
 * IfUnsignedGreaterGo instruction - BRANCH class
 *
 * IF>>GO - If Unsigned Greater Than, Go (Conditional Branch on Carry and Zero Flags)
 *
 * Mnemonic: IF>>GO
 * Format: IF>>GO <displacement>
 * Variants: 2
 * Operands: 1 (displacement)
 *
 * Opcodes:
 *   0x00D4 (IF>>GO:B) - Branch with byte displacement (-128 to +127)
 *   0x00D5 (IF>>GO:H) - Branch with halfword displacement (-32768 to +32767)
 *
 * Operation:
 *   if (C flag == 0 AND Z flag == 0) then
 *       PC = PC + sign_extend(displacement)
 *   else
 *       PC = next instruction
 *   endif
 *
 * Description:
 *   Performs a conditional branch if both the C (carry/borrow) flag is clear (C=0)
 *   AND the Z (zero) flag is clear (Z=0), indicating an unsigned greater-than
 *   condition from a prior comparison.
 *
 *   This instruction is used after a COMP (compare) instruction to branch based
 *   on unsigned integer comparison results. The combination C=0 AND Z=0 means
 *   no borrow occurred and the result was non-zero, which indicates the first
 *   operand was strictly greater than the second operand in unsigned arithmetic.
 *
 *   Typical usage pattern:
 *     COMP operand1, operand2    ; Compare: operand1 - operand2
 *     IF>>GO:B GREATER_LABEL     ; Branch if operand1 > operand2 (unsigned)
 *
 * Unsigned Comparison Logic:
 *   After COMP A, B (performs A - B):
 *   - C=0 (no borrow) AND Z=0 (non-zero) means A > B (unsigned)
 *   - C=1 (borrow occurred) means A < B (unsigned)
 *   - Z=1 means A == B (regardless of C)
 *   - C=0 means A >= B (unsigned)
 *
 * Branch Condition:
 *   Branches when C=0 AND Z=0 (unsigned strictly greater than after comparison)
 *
 * Displacement Encoding:
 *   - Byte displacement (0x00D4): Signed 8-bit (-128 to +127 bytes)
 *   - Halfword displacement (0x00D5): Signed 16-bit (-32768 to +32767 bytes)
 *   - Assembler auto-selects optimal size based on target distance
 *
 * Flags: None modified
 *   All flags (C, Z, S, V, K) remain unchanged by this instruction
 *
 * Trap conditions:
 *   - Addressing traps if target address is invalid
 *   - Branch trap (BT) if target protection violation
 *   - Page fault if target page not present
 *
 * Performance:
 *   - Branch taken: 1-2 cycles
 *   - Branch not taken: 1 cycle
 *
 * Key Characteristics:
 *   - Tests both carry and zero flags (compound condition)
 *   - Must follow COMP or similar instruction that sets C and Z flags
 *   - Unsigned comparison only (distinct from signed IF>GO)
 *   - Two displacement sizes for short/long jumps
 *   - Essential for unsigned range validation
 *   - Common in pointer and address comparisons
 *
 * Common Use Cases:
 *   - Unsigned loop bounds checking (counter > limit)
 *   - Pointer validation (ptr > min_address)
 *   - Memory address range validation
 *   - Unsigned threshold detection
 *   - Array index validation
 *   - Buffer capacity checks
 *
 * Example Usage:
 *   ; Check if value exceeds threshold
 *   COMP VALUE, THRESHOLD      ; Compare value with threshold
 *   IF>>GO:B ABOVE_THRESHOLD   ; Branch if value > threshold (unsigned)
 *   ; Value is within threshold
 *   ...
 *   ABOVE_THRESHOLD:
 *   ; Handle exceeded threshold
 *
 *   ; Validate pointer range
 *   COMP POINTER, MIN_ADDRESS
 *   IF<<GO:B INVALID_PTR       ; Branch if ptr < min (unsigned)
 *   COMP POINTER, MAX_ADDRESS
 *   IF>>GO:B INVALID_PTR       ; Branch if ptr > max (unsigned)
 *   ; Pointer is in valid range
 *   ...
 *   INVALID_PTR:
 *   ; Handle out-of-range pointer
 *
 * Related Instructions:
 *   - IF<<GO: Branch if unsigned less than (C=1)
 *   - IF<<=GO: Branch if unsigned less or equal (C=1 OR Z=1)
 *   - IF>>=GO: Branch if unsigned greater or equal (C=0)
 *   - COMP: Compare instruction that sets flags
 *   - IF>GO, IF<GO: Signed comparison branches
 */
void nd500_instr_IfUnsignedGreaterGo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count (should have 1 displacement operand)
    if (fi->operand_count != 1) {
        printf("[ERROR] IF>>GO at PC=0x%08X: Expected 1 operand (displacement), got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Test C and Z flags for unsigned greater-than condition
    // ND-500: After COMP A, B: C=1 means A >= B (no borrow), C=0 means A < B (borrow)
    // Z=0 means non-zero result (operand1 != operand2)
    // Combined: C=1 AND Z=0 means operand1 > operand2 (unsigned)
    bool carry_set = nd500_test_flag(cpu, ND500_FLAG_C);
    bool zero_clear = !nd500_test_flag(cpu, ND500_FLAG_Z);

    if (carry_set && zero_clear) {
        // Read displacement value and sign-extend based on data type
        uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        int64_t displacement = nd500_sign_extend_by_dtype(value, fi->data_type);

        // Update PC (relative branch from instruction start)
        cpu->PC = (uint32_t)(fi->address + displacement);
    }
    // else: C=1 OR Z=1 (A < B or A == B), branch not taken

    // No status flags are modified by this instruction
}
