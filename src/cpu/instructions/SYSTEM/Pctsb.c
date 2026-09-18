#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_tlb.h"
#include <stdio.h>
#include <stdlib.h>
#include "nd500_settings.h"   /* emulator knobs, as plain fields */

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
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Pctsb.cs
 */
void nd500_instr_Pctsb(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (must be 0) */
    if (fi->operand_count != 0) {
        printf("[ERROR] PCTSB expects 0 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - PCTSB requires PIA bit set */
    if (!(cpu->ST1 & (1u << ND500_ST_BIT_PIA))) {
        printf("[ERROR] PCTSB requires privileged mode at PC=0x%08X\n", fi->address);
        trap_illegal_instruction(cpu, fi->address, fi->opcode);
        return;
    }

    /* nd500x now HAS a translation cache (src/cpu/nd500_tlb.h), so this
     * is no longer a no-op: honour the instruction and drop every cached
     * translation. The cache also self-invalidates on writes to any page
     * a walk read a table from, so this is belt-and-braces rather than
     * the sole guarantee - which matters, because locore.s admits the
     * kernel's own dctsb placement was "not consistent". */
    nd500_mmu_tlb_flush();

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

    {
        static int dbg = -1;
        if (dbg < 0) dbg = nd500_settings()->tsbdbg;
        if (dbg)
            printf("[PCTSB] Program TLB clear (emulator no-op) at PC=0x%08X\n", fi->address);
    }

    /* ========================================================================
     * WHY IS TLB CLEARING A NO-OP IN THE EMULATOR?
     * ========================================================================
     *
     * This is a common question! Here's the detailed explanation:
     *
     * 1. WHAT IS A TLB AND WHY DOES REAL HARDWARE NEED IT?
     * -------------------------------------------------------
     * On real ND-500 hardware, every memory access requires address translation
     * through the MMU (Memory Management Unit):
     *
     *   Step 1: Read capability table entry     (1 memory access)
     *   Step 2: Read PST (Physical Segment)     (1 memory access)
     *   Step 3: Read page table (if paged)      (1-2 memory accesses)
     *   Step 4: Finally access the actual data  (1 memory access)
     *   --------------------------------------------------------
     *   TOTAL: 3-5 memory accesses PER instruction fetch = EXTREMELY SLOW!
     *
     * Without a TLB, every instruction fetch would require 3-5 memory lookups
     * just to find where the instruction is! The ND-500 would be unusably slow.
     *
     * The TLB (Translation Lookaside Buffer) is a HARDWARE CACHE that stores
     * recent address translations:
     *   - Maps: (virtual_page) -> (physical_page, permissions)
     *   - Hardware lookup in ~1 CPU cycle (associative memory)
     *   - Typical hit rate: 95-99%
     *   - When TLB hits: Skip all table lookups, use cached translation
     *   - When TLB misses: Do full table lookup, then cache the result
     *
     * This is why DCTSB/PCTSB exist - to invalidate stale TLB entries when
     * page tables or capabilities change (process switch, page table update, etc.)
     *
     * 2. WHY DOESN'T THE EMULATOR IMPLEMENT A TLB?
     * ----------------------------------------------
     * Reason A: The HOST CPU already has a TLB!
     *
     *   The emulator runs on a modern x86/ARM CPU that ALREADY HAS its own TLB
     *   for host memory. When the emulator's TranslateVirtualAddress() function
     *   accesses the capability tables, PST, and page tables in the emulated
     *   memory array, the HOST CPU's TLB is already caching those accesses!
     *
     *   Emulated ND-500 Memory -> Host RAM -> Host CPU TLB (already optimizing!)
     *
     * Reason B: Emulation is already slow - MMU overhead is negligible
     *
     *   The emulator is interpreting ND-500 instructions in software. Each
     *   ND-500 instruction might execute 100+ host CPU instructions. The MMU
     *   translation overhead is negligible compared to interpretation overhead.
     *
     * Reason C: Adding an emulated TLB would provide minimal benefit
     *
     *   - Would add complex cache coherency logic
     *   - Would add invalidation tracking code
     *   - Would add TLB entry management
     *   - Would make debugging harder (cache-related bugs are nasty!)
     *   - Would provide maybe 2-5% speedup at best
     *   - NOT WORTH THE COMPLEXITY
     *
     * 3. WHY CORRECTNESS > PERFORMANCE FOR EMULATION
     * ------------------------------------------------
     * The emulator's goal is ACCURATE ND-500 BEHAVIOR, not speed.
     *
     * Doing full table lookup every time:
     *   OK Ensures we catch bugs in page table setup
     *   OK Makes memory access behavior deterministic
     *   OK Simplifies debugging (no cache-related heisenbugs)
     *   OK Matches hardware behavior functionally (just slower)
     *
     * 4. WHY PRIVILEGE CHECKING IS STILL CRITICAL
     * ---------------------------------------------
     * Even though TLB clearing is a no-op, the privilege check is ESSENTIAL:
     *
     *   a) OS Code Testing: If OS code calls PCTSB without setting PIA first,
     *      it's a BUG that would fail on real hardware. The emulator MUST catch this!
     *
     *   b) Security Testing: If unprivileged code tries to execute PCTSB, it
     *      MUST trap. This validates security assumptions in the OS.
     *
     *   c) Behavioral Compatibility: Software running on the emulator should
     *      behave IDENTICALLY to real hardware, just slower. All traps must
     *      occur at the same points.
     *
     * SUMMARY
     * -------
     * The emulator models BEHAVIOR, not PERFORMANCE CHARACTERISTICS.
     *
     * TLB clearing is a no-op because:
     *   - Emulator has no TLB to clear (always does full translation)
     *   - Host CPU already optimizes the memory accesses with its own TLB
     *   - Performance isn't critical for emulation
     *   - Correctness and simplicity are more important
     *
     * But privilege checking is NOT a no-op because:
     *   - OS code must be tested for correct privilege handling
     *   - Security validation requires proper traps
     *   - Behavioral compatibility with real hardware is essential
     *
     * The emulator is functionally correct - it just doesn't optimize what
     * the hardware optimizes.
     * ======================================================================== */

    /* No status bits affected */
}
