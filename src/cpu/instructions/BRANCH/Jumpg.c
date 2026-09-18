/*
 * Jumpg.c - ND-500 Jumpg instruction (BRANCH class)
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
 * Jumpg instruction - BRANCH class
 *
 * JUMPG - Jump General (Unconditional absolute jump)
 *
 * Mnemonic: JUMPG
 * Format: JUMPG <address>
 * Variants: 1
 * Operands: 1 (<address/r/W>)
 *
 * Opcode:
 *   0x00B4 (JUMPG) - Jump to absolute address
 *
 * Operation:
 *   PC = address
 *
 * Description:
 *   Performs an unconditional jump to the absolute address specified by the
 *   operand. The operand is always treated as a word (32-bit) value representing
 *   the target address in the ND-500 memory space.
 *
 *   Unlike relative branch instructions (GO, IF conditions), JUMPG uses an
 *   absolute address and does not calculate offsets. This makes it ideal for
 *   long-distance jumps, computed jumps through address tables, and indirect
 *   jumps where the target address is calculated at runtime.
 *
 *   The instruction supports general addressing modes, allowing jumps to:
 *   - Immediate addresses (JUMPG 0x1000)
 *   - Addresses in registers (JUMPG I1)
 *   - Addresses in memory (JUMPG (I2))
 *   - Indexed addresses (JUMPG TABLE(I3))
 *
 *   Special behavior: The address operand may NOT be prefixed with the ALT
 *   (alternative addressing) prefix. Attempting to use ALT results in an
 *   Illegal Operand Specifier (IOS) trap.
 *
 *   Descriptor Range Trap Handling: If a descriptor range trap occurs during
 *   operand evaluation, execution "falls through" to the next instruction
 *   rather than jumping. This allows error recovery without infinite loops.
 *
 * Address Space:
 *   - Valid addresses: 0x00000000 to 0xFFFFFFFF (full 32-bit space)
 *   - Byte-addressed memory (any byte-aligned address is valid)
 *   - Even addresses strongly recommended for instruction fetch
 *   - MMU may restrict accessible ranges based on process permissions
 *
 * Addressing Mode Restrictions:
 *   - ALT prefix: NOT ALLOWED (raises IOS trap)
 *   - All other general addressing modes: ALLOWED
 *   - Common modes: immediate, register, memory, indexed, preindexed
 *
 * Flags: None modified
 *   All status flags (C, Z, S, V, K) remain unchanged
 *
 * Trap conditions:
 *   - IOS (Illegal Operand Specifier) if ALT prefix used
 *   - BT (Branch Trap) if target address protection violation
 *   - Addressing traps for operand access
 *   - Descriptor range trap (execution falls through on trap)
 *   - Page fault if target page not present
 *
 * Performance:
 *   - Typical: 2-3 cycles
 *   - Best case: 2 cycles (immediate or register operand)
 *   - Worst case: 3+ cycles (indexed memory operand)
 *   - Pipeline may be flushed on jump
 *
 * Key Characteristics:
 *   - Unconditional execution (always jumps)
 *   - Absolute addressing (not PC-relative)
 *   - Supports all general addressing modes except ALT
 *   - Falls through on descriptor range trap
 *   - No flags modified
 *   - Essential for long-distance jumps
 *   - Enables computed/indirect jumps
 *   - Jump tables and switch statements
 *
 * Common Use Cases:
 *   - Computed jumps via address tables
 *   - Switch/case statement implementation
 *   - Indirect function calls (jump to address in register)
 *   - Long-distance jumps beyond relative branch range
 *   - Jump tables for dispatch loops
 *   - Dynamic code execution
 *   - Exception vector jumps
 *   - Far jumps across memory segments
 *
 * Example Usage:
 *   ; Direct absolute jump
 *   JUMPG 0x10000            ; Jump to address 0x10000
 *
 *   ; Jump via register
 *   I1 = TARGET_ADDRESS      ; Load target address
 *   JUMPG I1                 ; Jump to address in I1
 *
 *   ; Jump via memory
 *   JUMPG (ENTRY_POINT)      ; Jump to address stored at ENTRY_POINT
 *
 *   ; Jump table implementation
 *   I1 = CASE_INDEX          ; Get case index (0, 1, 2, ...)
 *   I1 = I1 * 4               ; Scale by 4 (word size)
 *   JUMPG JUMP_TABLE(I1)     ; Jump to address in table[index]
 *
 *   ; Indirect function call pattern
 *   I2 = FUNC_PTR            ; Load function pointer
 *   JUMPG I2                 ; Jump to function
 *   ; ... function code ...
 *   JUMPG RETURN_ADDRESS     ; Return via absolute jump
 *
 * Related Instructions:
 *   - GO: Unconditional relative branch (PC-relative)
 *   - JUMPS: Jump Short (optimized encoding, same behavior)
 *   - IF conditions: Conditional relative branches
 *   - CALL: Subroutine call with return address save
 *
 * Comparison with Related Instructions:
 *   - JUMPG vs GO: Absolute vs relative addressing
 *   - JUMPG vs JUMPS: Same behavior, different encoding optimization
 *   - JUMPG vs CALL: Jump without saving return address
 *   - JUMPG vs IF: Unconditional vs conditional execution
 *
 * Typical Pattern (Switch Statement):
 *   ; Switch on value in I1 (0-3)
 *   COMP I1, 3               ; Check if value > 3
 *   IF>GO:B DEFAULT_CASE     ; Handle out of range
 *   I2 = I1                  ; Copy index
 *   I2 = I2 * 4               ; Scale by word size
 *   JUMPG JUMP_TABLE(I2)     ; Jump to case handler
 *
 *   JUMP_TABLE:
 *   .WORD CASE_0_HANDLER
 *   .WORD CASE_1_HANDLER
 *   .WORD CASE_2_HANDLER
 *   .WORD CASE_3_HANDLER
 *
 * Security Considerations:
 *   - Jump target validation recommended for untrusted inputs
 *   - Bounds checking on jump table indices essential
 *   - BT trap protects against invalid memory access
 *   - MMU enforces address space isolation
 */
void nd500_instr_Jumpg(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 1) {
        printf("[ERROR] JUMPG at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read absolute target address (always word-sized)
    uint64_t address_raw = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    uint32_t target_address = (uint32_t)(address_raw & 0xFFFFFFFF);

    /* The target lives in DATA memory, so this read can page-fault. raise_trap
     * dispatches the fault synchronously: it installs the handler PC and clears
     * the trap state. Writing cpu->PC below would then OVERWRITE the handler's
     * PC with a garbage target read from an unmapped page, and the CPU would
     * execute that address in KERNEL context with in_trap_handler set.
     *
     * This was the native assembler's "ENTS at PC=0x0001595E: Must be preceded
     * by CALL/CALLG" halt. The PC ring (ND500X_STOPDBG) showed user PC 0x309D
     * (CED=6, inH=0) - a JUMPG whose operand faulted at 0x1E28 - stepping
     * straight to 0x1595E (CED=0, CAD=6, inH=1) even though the dispatch had
     * set PC=0x381. The ENTS then raised a FALSE ISE because trap dispatch had
     * cleared the CALL/ENT sequence interlock.
     *
     * Aborting here lets the handler run; the jump re-executes after RETT. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    // Set PC to absolute address (unconditional jump)
    cpu->PC = target_address;

    // Note: Status flags are not modified by JUMPG
    // Note: ALT prefix checking would be done by operand decoder (IOS trap)
    // Note: BT (branch trap) checking would be done by trap/MMU system
    // Note: Descriptor range trap handling would fall through to next instruction
}
