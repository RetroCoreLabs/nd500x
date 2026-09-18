/*
 * IfUnsignedLessGo.c - ND-500 IfUnsignedLessGo instruction (BRANCH class)
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
 * IfUnsignedLessGo instruction - BRANCH class
 *
 * IF<<GO - If Unsigned Less Than, Go (Conditional Branch on Carry Flag)
 *
 * Mnemonic: IF<<GO
 * Format: IF<<GO <displacement>
 * Variants: 2
 * Operands: 1 (displacement)
 *
 * Opcodes:
 *   0x00D8 (IF<<GO:B) - Branch with byte displacement (-128 to +127)
 *   0x00D9 (IF<<GO:H) - Branch with halfword displacement (-32768 to +32767)
 *
 * Operation:
 *   if (C flag == 1) then
 *       PC = PC + sign_extend(displacement)
 *   else
 *       PC = next instruction
 *   endif
 *
 * Description:
 *   Performs a conditional branch if the C (carry/borrow) flag is set (C=1),
 *   indicating an unsigned less-than condition from a prior comparison.
 *
 *   This instruction is used after a COMP (compare) instruction to branch
 *   based on unsigned integer comparison results. The C flag is set when
 *   a borrow occurred during subtraction, which means the first operand
 *   was less than the second operand in unsigned arithmetic.
 *
 *   Typical usage pattern:
 *     COMP operand1, operand2    ; Compare: operand1 - operand2
 *     IF<<GO:B LESS_THAN_LABEL  ; Branch if operand1 < operand2 (unsigned)
 *
 * Unsigned Comparison Logic:
 *   After COMP A, B (performs A - B):
 *   - C=0 (borrow occurred) means A < B (unsigned)
 *   - C=1 (no borrow) means A >= B (unsigned)
 *   - Z=1 additionally means A == B
 *
 * Branch Condition:
 *   Branches when C=0 (unsigned less than after comparison)
 *
 * Displacement Encoding:
 *   - Byte displacement (0x00D8): Signed 8-bit (-128 to +127 bytes)
 *   - Halfword displacement (0x00D9): Signed 16-bit (-32768 to +32767 bytes)
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
 *   - Independent of sign flag (unsigned comparison only)
 *   - Two displacement sizes for short/long jumps
 *   - Essential for unsigned array bounds checking
 *   - Common in pointer arithmetic and address validation
 *
 * Common Use Cases:
 *   - Unsigned array bounds checking (index < size)
 *   - Pointer comparison and validation
 *   - Memory address range checking
 *   - Unsigned counter comparisons
 *   - Multi-precision arithmetic comparisons
 *   - Buffer overflow prevention checks
 *
 * Example Usage:
 *   ; Array bounds check
 *   COMP INDEX, ARRAY_SIZE    ; Compare index with size
 *   IF<<GO:B OUT_OF_BOUNDS    ; Branch if index < size (invalid)
 *   ; Index is valid, continue
 *   ...
 *   OUT_OF_BOUNDS:
 *   ; Handle bounds error
 *
 *   ; Pointer validation
 *   COMP POINTER, MIN_ADDRESS
 *   IF<<GO:B INVALID_PTR      ; Branch if ptr < min (unsigned)
 *   COMP POINTER, MAX_ADDRESS
 *   IF>>GO:B INVALID_PTR      ; Branch if ptr > max (unsigned)
 *   ; Pointer is in valid range
 *
 * Related Instructions:
 *   - IF>>GO: Branch if unsigned greater than (C=1 AND Z=0)
 *   - IF<<=GO: Branch if unsigned less or equal (C=0 OR Z=1)
 *   - IF>>=GO: Branch if unsigned greater or equal (C=1)
 *   - COMP: Compare instruction that sets flags
 *   - IF<GO, IF>GO: Signed comparison branches
 */
void nd500_instr_IfUnsignedLessGo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count (should have 1 displacement operand)
    if (fi->operand_count != 1) {
        printf("[ERROR] IF<<GO at PC=0x%08X: Expected 1 operand (displacement), got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Test C (carry/borrow) flag for unsigned less-than condition
    // ND-500: After COMP A, B: C=1 means A >= B (no borrow), C=0 means A < B (borrow)
    // Branch when C=0 (borrow occurred, meaning A < B unsigned)
    if (!nd500_test_flag(cpu, ND500_FLAG_C)) {
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
    // else: C=0, no borrow (A >= B), branch not taken

    // No status flags are modified by this instruction
}
