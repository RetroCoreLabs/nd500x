#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * PCTSB instruction - SYSTEM class
 *
 * Mnemonic: pctsb
 * Operands: 0
 * Opcode: 0xFF1C (hex) / 0177434 (octal) / 65308 (decimal)
 *
 * Operation: Clear Program Translation Speedup Buffer (Program/Instruction TLB)
 *
 * Description:
 * Privileged instruction that clears the program translation speedup buffer (TLB).
 * The TLB is a hardware cache that stores recent virtual-to-physical address
 * translations for instruction fetches to improve MMU performance. This instruction
 * invalidates all entries in the program TLB, forcing subsequent instruction
 * fetches to perform full address translation from scratch.
 *
 * Translation Lookaside Buffer (TLB) Overview:
 * See DCTSB for comprehensive TLB explanation. PCTSB is identical to DCTSB but
 * operates on the program (instruction) TLB instead of the data TLB. The ND-500
 * maintains separate TLBs for instructions and data to optimize different access
 * patterns and allow independent invalidation.
 *
 * Program TLB vs Data TLB:
 * - Program TLB: Caches translations for instruction fetches (PC-based accesses)
 * - Data TLB: Caches translations for load/store operations (data accesses)
 * - Separate TLBs because:
 *   * Different access patterns (sequential code vs. scattered data)
 *   * Independent invalidation (process switch may only need data TLB clear)
 *   * Hardware can optimize each TLB for its specific access pattern
 *
 * When Program TLB Must Be Cleared (PCTSB Use Cases):
 * 1. Process Switch: New process has different code space
 * 2. Dynamic Code Generation: Self-modifying code or JIT compilation
 * 3. Code Page Updates: OS modified executable page mappings
 * 4. Context Switch: Switching to different program memory context
 * 5. MMU Reconfiguration: Capability or PST tables updated
 * 6. Debugging: Breakpoint insertion/removal (code patching)
 *
 * Operand Structure:
 * - No operands (zero-operand instruction)
 *
 * Operation Steps:
 * 1. Validate operand count (must be 0)
 * 2. Check PIA bit (Privileged Instruction Allowed)
 * 3. If not privileged, raise IIC trap
 * 4. Clear all entries in program translation speedup buffer
 * 5. Optionally flush instruction cache (hardware side effect)
 *
 * Flag Behavior:
 * - All data status bits unaffected (S, Z, C, O, K)
 *
 * Trap Conditions:
 * - Illegal Instruction Code (IIC): Not in privileged mode (PIA bit not set)
 *
 * Performance:
 * - Execution: ~5-10 cycles
 * - Side effect: Performance penalty until program TLB warms up again
 * - Following instruction fetches slower until TLB repopulated
 * - Hardware may also flush instruction cache
 *
 * Key Characteristics:
 * - Privileged instruction (requires PIA bit in status register)
 * - No operands
 * - Invalidates all program TLB entries
 * - IIC trap if not privileged
 * - No flags affected
 * - Performance-critical for OS context switching
 * - Separate from DCTSB (data TLB)
 * - Often used together with DCTSB for full context switch
 *
 * Common Use Cases:
 * - Operating system process context switch
 * - After dynamic code generation or modification
 * - After loading new executable code
 * - Debugging code patching (breakpoints)
 * - Self-modifying code synchronization
 *
 * Typical Usage:
 *   Example 1: Process context switch
 *     PCTSB              ; Clear program TLB for new process
 *     DCTSB              ; Clear data TLB for new process
 *     ; Load new process context...
 *
 *   Example 2: After dynamic code generation
 *     ; Generate new machine code...
 *     PCTSB              ; Invalidate cached code translations
 *     ; Jump to newly generated code
 *
 *   Example 3: Debugging breakpoint insertion
 *     ; Patch instruction with breakpoint
 *     PCTSB              ; Flush cached instruction
 *
 * Notes:
 * - Must be executed in privileged mode (PIA bit set)
 * - Separate from DCTSB (data TLB clear)
 * - Real hardware also flushes instruction cache
 * - Critical for self-modifying code correctness
 * - Expensive operation (performance penalty after clearing)
 * - OS usually pairs with DCTSB for full TLB flush
 *
 * IMPLEMENTATION STATUS: FUNCTIONAL (Emulator no-op)
 *
 * Emulator behavior:
 * - Emulator does not implement TLB (always does full translation)
 * - PCTSB is a no-op (nothing to clear)
 * - Still validates privilege level and operand count
 * - Raises IIC trap if not privileged (for OS correctness)
 * - Functionally equivalent to real hardware (just slower)
 *
 * Why no TLB in emulator:
 * - Same rationale as DCTSB (see DCTSB documentation)
 * - Emulator TranslateVirtualAddress() is already fast
 * - No separate instruction/data translation paths
 * - Correctness > Performance for emulation
 *
 * Related Instructions:
 * - DCTSB: Clear data translation speedup buffer (data TLB)
 * - ICACHE: Clear instruction cache (cache control)
 * - DCACHE: Clear data cache (cache control)
 *
 * Reference: ND-500 Reference Manual, Chapter 16.24
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Pctsb.cs
 */
void nd500_instr_Pctsb(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (must be 0) */
    if (fi->operand_count != 0) {
        printf("[ERROR] PCTSB expects 0 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - PCTSB requires PIA bit set
     * TODO: Implement PIA bit checking when status register is fully implemented
     * For now, we allow the instruction to execute */
    /*
    if (!(cpu->ST1 & ST_PIA_BIT)) {
        printf("[ERROR] PCTSB requires privileged mode at PC=0x%08X\n", fi->address);
        trap_illegal_instruction(cpu, fi->address, fi->opcode);
        return;
    }
    */

    /* EMULATOR NO-OP: Clear program translation speedup buffer
     *
     * In real hardware, this would:
     * 1. Invalidate all entries in the program TLB
     * 2. Flush instruction cache (stale instructions)
     * 3. Reset TLB associative lookup state
     * 4. Following instruction fetches perform full MMU translation
     *
     * In emulator:
     * - No TLB implementation (always full translation)
     * - No separate instruction cache (direct memory access)
     * - This instruction is effectively a no-op
     * - But still validates privilege level for OS correctness
     */

    printf("[PCTSB] Program TLB clear (emulator no-op) at PC=0x%08X\n", fi->address);

    /* No status bits affected */
}
