/*
 * IfUnsignedGreaterEqualGo.c - ND-500 IfUnsignedGreaterEqualGo instruction (BRANCH class)
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
 * IfUnsignedGreaterEqualGo instruction - BRANCH class
 *
 * IF>>=GO - If Unsigned Greater or Equal, Go (Conditional Branch on Carry Flag)
 *
 * Mnemonic: IF>>=GO
 * Format: IF>>=GO <displacement>
 * Variants: 2
 * Operands: 1 (displacement)
 *
 * Opcodes:
 *   0x00D6 (IF>>=GO:B) - Branch with byte displacement (-128 to +127)
 *   0x00D7 (IF>>=GO:H) - Branch with halfword displacement (-32768 to +32767)
 *
 * Operation:
 *   if (C flag == 0) then
 *       PC = PC + sign_extend(displacement)
 *   else
 *       PC = next instruction
 *   endif
 *
 * Description:
 *   Performs a conditional branch if the C (carry/borrow) flag is clear (C=0),
 *   indicating an unsigned greater-than-or-equal condition from a prior comparison.
 *
 *   This instruction is used after a COMP (compare) instruction to branch based
 *   on unsigned integer comparison results. The C flag being clear means no
 *   borrow occurred during subtraction, which indicates the first operand was
 *   greater than or equal to the second operand in unsigned arithmetic.
 *
 *   Typical usage pattern:
 *     COMP operand1, operand2          ; Compare: operand1 - operand2
 *     IF>>=GO:B GREATER_EQUAL_LABEL   ; Branch if operand1 >= operand2 (unsigned)
 *
 * Unsigned Comparison Logic:
 *   After COMP A, B (performs A - B):
 *   - C=1 (no borrow) means A >= B (unsigned)
 *   - C=0 (borrow occurred) means A < B (unsigned)
 *   - Z=1 additionally indicates A == B
 *   - C=1 AND Z=0 means A > B (strictly greater)
 *
 * Branch Condition:
 *   Branches when C=1 (unsigned greater than or equal after comparison)
 *
 * Displacement Encoding:
 *   - Byte displacement (0x00D6): Signed 8-bit (-128 to +127 bytes)
 *   - Halfword displacement (0x00D7): Signed 16-bit (-32768 to +32767 bytes)
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
 *   - Tests carry flag for unsigned comparison result
 *   - Must follow COMP or similar instruction that sets C flag
 *   - Independent of zero flag (handles both > and == cases)
 *   - Unsigned comparison only (distinct from signed IF>=GO)
 *   - Two displacement sizes for short/long jumps
 *   - Essential for unsigned minimum validation
 *   - Common in pointer and bounds checking
 *
 * Common Use Cases:
 *   - Unsigned minimum bounds checking (value >= minimum)
 *   - Pointer lower bound validation (ptr >= min_address)
 *   - Array index validation (index >= 0, though always true for unsigned)
 *   - Buffer start position validation
 *   - Unsigned threshold checking (count >= required)
 *   - Memory address lower bound enforcement
 *
 * Example Usage:
 *   ; Validate minimum threshold
 *   COMP VALUE, MIN_THRESHOLD  ; Compare value with minimum
 *   IF>>=GO:B VALUE_OK        ; Branch if value >= minimum (unsigned)
 *   ; Value below minimum
 *   JMP ERROR_TOO_SMALL
 *   VALUE_OK:
 *   ; Value meets minimum requirement
 *
 *   ; Pointer lower bound check
 *   COMP POINTER, MIN_ADDRESS
 *   IF>>=GO:B PTR_VALID       ; Branch if ptr >= min (unsigned)
 *   ; Pointer below valid range
 *   JMP INVALID_POINTER
 *   PTR_VALID:
 *   ; Pointer is at or above minimum
 *
 *   ; Combined range validation
 *   COMP VALUE, MIN_VALUE
 *   IF<<GO:B OUT_OF_RANGE     ; Branch if value < min (using IF<<GO)
 *   COMP VALUE, MAX_VALUE
 *   IF>>GO:B OUT_OF_RANGE     ; Branch if value > max (using IF>>GO)
 *   ; Value is in valid range [min, max]
 *   ...
 *   OUT_OF_RANGE:
 *   ; Handle out-of-range value
 *
 * Related Instructions:
 *   - IF<<GO: Branch if unsigned less than (C=0)
 *   - IF>>GO: Branch if unsigned greater than (C=1 AND Z=0)
 *   - IF<<=GO: Branch if unsigned less or equal (C=0 OR Z=1)
 *   - COMP: Compare instruction that sets flags
 *   - IF>=GO, IF<=GO: Signed comparison branches
 */
void nd500_instr_IfUnsignedGreaterEqualGo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count (should have 1 displacement operand)
    if (fi->operand_count != 1) {
        printf("[ERROR] IF>>=GO at PC=0x%08X: Expected 1 operand (displacement), got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Test C (carry) flag for unsigned greater-or-equal condition
    // ND-500: After COMP A, B: C=1 means A >= B (no borrow), C=0 means A < B (borrow)
    // Branch when C=1 (no borrow, meaning A >= B unsigned)
    if (nd500_test_flag(cpu, ND500_FLAG_C)) {
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
    // else: C=1, borrow occurred (A < B), branch not taken

    // No status flags are modified by this instruction
}
