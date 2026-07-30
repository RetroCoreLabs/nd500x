#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Jumps instruction - BRANCH class
 *
 * JUMPS - Jump Short (Unconditional absolute jump, optimized encoding)
 *
 * Mnemonic: JUMPS
 * Format: JUMPS <address>
 * Variants: 1
 * Operands: 1 (<address/r/W>)
 *
 * Opcode:
 *   0x00B9 (JUMPS) - Jump to absolute address (short form)
 *
 * Operation:
 *   PC = address
 *
 * Description:
 *   Performs an unconditional jump to the absolute address specified by the
 *   operand. JUMPS is functionally identical to JUMPG but uses an optimized
 *   instruction encoding for shorter code size and potentially faster decoding.
 *
 *   The operand is always treated as a word (32-bit) value representing the
 *   target address in the ND-500 memory space. The instruction supports the
 *   same general addressing modes as JUMPG, making it suitable for all types
 *   of absolute jumps.
 *
 *   The "short" designation refers to the instruction encoding optimization,
 *   not a limitation on jump distance. JUMPS can reach any address in the
 *   32-bit address space, just like JUMPG.
 *
 *   Assemblers typically select between JUMPG and JUMPS automatically based
 *   on context and optimization preferences. The choice does not affect
 *   program behavior or semantics, only encoding efficiency.
 *
 *   The instruction supports general addressing modes, allowing jumps to:
 *   - Immediate addresses (JUMPS 0x1000)
 *   - Addresses in registers (JUMPS I1)
 *   - Addresses in memory (JUMPS (I2))
 *   - Indexed addresses (JUMPS TABLE(I3))
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
 *   - May be slightly faster than JUMPG due to optimized encoding
 *
 * Key Characteristics:
 *   - Unconditional execution (always jumps)
 *   - Absolute addressing (not PC-relative)
 *   - Supports all general addressing modes except ALT
 *   - Falls through on descriptor range trap
 *   - No flags modified
 *   - Optimized encoding for code size efficiency
 *   - Functionally identical to JUMPG
 *   - Enables computed/indirect jumps
 *
 * Common Use Cases:
 *   - All same use cases as JUMPG
 *   - Preferred by assemblers for code size optimization
 *   - Computed jumps via address tables
 *   - Switch/case statement implementation
 *   - Indirect function calls
 *   - Long-distance jumps
 *   - Jump tables for dispatch loops
 *   - Exception vector jumps
 *
 * Example Usage:
 *   ; Direct absolute jump
 *   JUMPS 0x10000            ; Jump to address 0x10000
 *
 *   ; Jump via register
 *   I1 = TARGET_ADDRESS      ; Load target address
 *   JUMPS I1                 ; Jump to address in I1
 *
 *   ; Jump via memory
 *   JUMPS (ENTRY_POINT)      ; Jump to address stored at ENTRY_POINT
 *
 *   ; Jump table implementation
 *   I1 = CASE_INDEX          ; Get case index (0, 1, 2, ...)
 *   I1 = I1 * 4               ; Scale by 4 (word size)
 *   JUMPS JUMP_TABLE(I1)     ; Jump to address in table[index]
 *
 *   ; Indirect function call pattern
 *   I2 = FUNC_PTR            ; Load function pointer
 *   JUMPS I2                 ; Jump to function
 *   ; ... function code ...
 *   JUMPS RETURN_ADDRESS     ; Return via absolute jump
 *
 * Related Instructions:
 *   - JUMPG: Jump General (same behavior, different encoding)
 *   - GO: Unconditional relative branch (PC-relative)
 *   - IF conditions: Conditional relative branches
 *   - CALL: Subroutine call with return address save
 *
 * Comparison with Related Instructions:
 *   - JUMPS vs JUMPG: Same behavior, JUMPS has optimized encoding
 *   - JUMPS vs GO: Absolute vs relative addressing
 *   - JUMPS vs CALL: Jump without saving return address
 *   - JUMPS vs IF: Unconditional vs conditional execution
 *
 * Encoding Optimization:
 *   The "short" designation indicates an optimized instruction encoding
 *   that may result in:
 *   - Smaller instruction size for certain operand types
 *   - Faster decoding in CPU pipeline
 *   - Better code density in memory-constrained environments
 *   - No functional difference from JUMPG
 *
 * Assembler Selection:
 *   Most assemblers automatically choose between JUMPG and JUMPS based on:
 *   - Operand addressing mode
 *   - Code optimization flags
 *   - Instruction encoding efficiency
 *   Programmers typically write "JUMP" and let the assembler select the
 *   optimal variant.
 *
 * Security Considerations:
 *   - Jump target validation recommended for untrusted inputs
 *   - Bounds checking on jump table indices essential
 *   - BT trap protects against invalid memory access
 *   - MMU enforces address space isolation
 */
void nd500_instr_Jumps(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 1) {
        printf("[ERROR] JUMPS at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read absolute target address (always word-sized)
    uint64_t address_raw = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);

    /* The target lives in DATA memory, so this read can page-fault. raise_trap
     * dispatches the fault synchronously - installing the handler PC and
     * clearing the trap state - so writing cpu->PC below would overwrite the
     * handler's PC with a garbage target read from an unmapped page, and the
     * CPU would execute it in kernel context. Identical defect to JUMPG, which
     * was proven to kill the native assembler; fixed there in dc2640c. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    uint32_t target_address = (uint32_t)(address_raw & 0xFFFFFFFF);

    // Set PC to absolute address (unconditional jump)
    cpu->PC = target_address;

    // Note: Status flags are not modified by JUMPS
    // Note: ALT prefix checking would be done by operand decoder (IOS trap)
    // Note: BT (branch trap) checking would be done by trap/MMU system
    // Note: Descriptor range trap handling would fall through to next instruction
}
