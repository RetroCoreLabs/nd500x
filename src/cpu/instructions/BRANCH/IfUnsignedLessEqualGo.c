/*
 * IfUnsignedLessEqualGo.c - ND-500 IfUnsignedLessEqualGo instruction (BRANCH class)
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
 * IfUnsignedLessEqualGo instruction - BRANCH class
 *
 * IF<<=GO - If Unsigned Less or Equal, Go (Conditional Branch on Carry or Zero)
 *
 * Mnemonic: IF<<=GO
 * Format: IF<<=GO <displacement>
 * Variants: 2
 * Operands: 1 (displacement)
 *
 * Opcodes:
 *   0x00DA (IF<<=GO:B) - Branch with byte displacement (-128 to +127)
 *   0x00DB (IF<<=GO:H) - Branch with halfword displacement (-32768 to +32767)
 *
 * Operation:
 *   if (C flag == 1 OR Z flag == 1) then
 *       PC = PC + sign_extend(displacement)
 *   else
 *       PC = next instruction
 *   endif
 *
 * Description:
 *   Performs a conditional branch if either the C (carry/borrow) flag is set (C=1)
 *   OR the Z (zero) flag is set (Z=1), indicating an unsigned less-than-or-equal
 *   condition from a prior comparison.
 *
 *   This instruction is used after a COMP (compare) instruction to branch based
 *   on unsigned integer comparison results. The condition C=1 OR Z=1 combines:
 *   - C=1: A borrow occurred (first operand < second operand, unsigned)
 *   - Z=1: Result was zero (first operand == second operand)
 *   Together these represent: first operand <= second operand (unsigned)
 *
 *   Typical usage pattern:
 *     COMP operand1, operand2      ; Compare: operand1 - operand2
 *     IF<<=GO:B LESS_EQUAL_LABEL  ; Branch if operand1 <= operand2 (unsigned)
 *
 * Unsigned Comparison Logic:
 *   After COMP A, B (performs A - B):
 *   - C=1 OR Z=1 means A <= B (unsigned)
 *   - C=1 alone means A < B (unsigned, non-equal)
 *   - Z=1 alone means A == B (equal)
 *   - C=0 AND Z=0 means A > B (unsigned)
 *
 * Branch Condition:
 *   Branches when C=1 OR Z=1 (unsigned less than or equal after comparison)
 *
 * Displacement Encoding:
 *   - Byte displacement (0x00DA): Signed 8-bit (-128 to +127 bytes)
 *   - Halfword displacement (0x00DB): Signed 16-bit (-32768 to +32767 bytes)
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
 *   - Tests carry OR zero flag (compound condition)
 *   - Must follow COMP or similar instruction that sets C and Z flags
 *   - Unsigned comparison only (distinct from signed IF<=GO)
 *   - Two displacement sizes for short/long jumps
 *   - Essential for loop termination conditions
 *   - Common in array bounds and limit checking
 *
 * Common Use Cases:
 *   - Unsigned loop termination (counter <= limit)
 *   - Array bounds validation (index <= max_index)
 *   - Pointer upper bound checking (ptr <= max_address)
 *   - Buffer capacity validation
 *   - Unsigned range checks (inclusive upper bound)
 *   - Memory limit enforcement
 *
 * Example Usage:
 *   ; Loop with inclusive upper bound
 *   COMP COUNTER, MAX_COUNT    ; Compare counter with maximum
 *   IF<<=GO:B LOOP_BODY        ; Continue if counter <= max (unsigned)
 *   ; Counter exceeded max, exit loop
 *   ...
 *   LOOP_BODY:
 *   ; Execute loop iteration
 *
 *   ; Array index validation (inclusive)
 *   COMP INDEX, LAST_INDEX     ; Compare with last valid index
 *   IF<<=GO:B INDEX_VALID      ; Branch if index <= last_index
 *   ; Index out of range
 *   JMP ERROR_HANDLER
 *   INDEX_VALID:
 *   ; Access array element
 *
 *   ; Pointer upper bound check
 *   COMP POINTER, MAX_ADDRESS
 *   IF<<=GO:B PTR_VALID        ; Branch if ptr <= max (unsigned)
 *   ; Pointer exceeds valid range
 *   ...
 *   PTR_VALID:
 *   ; Pointer is within valid range
 *
 * Related Instructions:
 *   - IF<<GO: Branch if unsigned less than (C=1)
 *   - IF>>GO: Branch if unsigned greater than (C=0 AND Z=0)
 *   - IF>>=GO: Branch if unsigned greater or equal (C=0)
 *   - COMP: Compare instruction that sets flags
 *   - IF<=GO, IF>=GO: Signed comparison branches
 */
void nd500_instr_IfUnsignedLessEqualGo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count (should have 1 displacement operand)
    if (fi->operand_count != 1) {
        printf("[ERROR] IF<<=GO at PC=0x%08X: Expected 1 operand (displacement), got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Test C and Z flags for unsigned less-than-or-equal condition
    // ND-500: After COMP A, B: C=1 means A >= B (no borrow), C=0 means A < B (borrow)
    // Z=1 means equal (zero result)
    // Combined: C=0 OR Z=1 means operand1 <= operand2 (unsigned)
    bool carry_clear = !nd500_test_flag(cpu, ND500_FLAG_C);
    bool zero_set = nd500_test_flag(cpu, ND500_FLAG_Z);

    if (carry_clear || zero_set) {
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
    // else: C=0 AND Z=0 (A > B), branch not taken

    // No status flags are modified by this instruction
}
