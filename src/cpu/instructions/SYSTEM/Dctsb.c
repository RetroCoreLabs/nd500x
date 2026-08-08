#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_tlb.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * DCTSB instruction - SYSTEM class
 *
 * Mnemonic: dctsb
 * Operands: 0
 * Opcode: 0xFF1D (hex) / 0177435 (octal) / 65309 (decimal)
 *
 * Operation: Clear Data Translation Speedup Buffer (Data TLB)
 *
 * Description:
 * Privileged instruction that clears the data translation speedup buffer (TLB).
 * The TLB is a hardware cache that stores recent virtual-to-physical address
 * translations for data accesses to improve MMU performance. This instruction
 * invalidates all entries in the data TLB, forcing subsequent data memory
 * accesses to perform full address translation from scratch.
 *
 * Translation Lookaside Buffer (TLB) Overview:
 * The TLB is a critical performance optimization in the ND-500 MMU. Without it,
 * every memory access requires multiple table lookups:
 *   1. Read capability table entry (1 memory access)
 *   2. Read PST (Physical Segment Table) entry (1 memory access)
 *   3. For paged segments, read page table entries (1-2 memory accesses)
 *   4. Finally access the actual data (1 memory access)
 *   Total: 3-5 memory accesses per data access = VERY SLOW!
 *
 * With TLB, the MMU caches recent translations:
 *   - TLB stores: (process#, domain#, virtual_page) -> (physical_page, permissions)
 *   - On memory access, check TLB first (hardware lookup, ~1 CPU cycle)
 *   - If TLB hit: Use cached translation immediately (FAST!)
 *   - If TLB miss: Do full table lookup, then cache result in TLB
 *   - Typical TLB hit rate: 95-99% = huge performance gain
 *
 * TLB Structure (Real ND-500 Hardware):
 * - Separate TLBs for program (instruction) and data accesses
 * - Each entry contains:
 *   * Process number (for multi-process systems)
 *   * Domain number (for multi-domain processes)
 *   * Virtual page number (upper 21 bits of 32-bit address)
 *   * Physical page number (translation result)
 *   * Permission bits (from capability: WRP, PAC, etc.)
 * - Typically 64-256 entries per TLB
 * - Associative/content-addressable lookup (parallel search of all entries)
 *
 * When TLB Must Be Cleared (DCTSB Use Cases):
 * 1. Process Switch: New process has different address space
 * 2. Domain Switch: New domain has different capability tables
 * 3. Page Table Updates: OS modified page mappings
 * 4. Context Switch: Switching to different memory context
 * 5. MMU Configuration Change: Capability or PST tables updated
 * 6. Cache Coherency: After DMA or I/O processor memory updates
 *
 * Data vs Program TLB:
 * - DCTSB: Clears data TLB (for load/store operations)
 * - PCTSB: Clears program TLB (for instruction fetches)
 * - Separate TLBs allow independent invalidation
 * - Different access patterns (sequential code vs. scattered data)
 *
 * Operand Structure:
 * - No operands (zero-operand instruction)
 *
 * Operation Steps:
 * 1. Validate operand count (must be 0)
 * 2. Check PIA bit (Privileged Instruction Allowed)
 * 3. If not privileged, raise IIC trap
 * 4. Clear all entries in data translation speedup buffer
 * 5. Optionally flush dirty cache lines to memory (hardware side effect)
 *
 * Flag Behavior:
 * - All data status bits unaffected (S, Z, C, O, K)
 *
 * Trap Conditions:
 * - Illegal Instruction Code (IIC): Not in privileged mode (PIA bit not set)
 *
 * Performance:
 * - Execution: ~5-10 cycles
 * - Side effect: Performance penalty until TLB warms up again
 * - Following memory accesses slower until TLB repopulated
 * - Hardware also flushes write-back data cache (dirty lines to memory)
 *
 * Key Characteristics:
 * - Privileged instruction (requires PIA bit in status register)
 * - No operands
 * - Invalidates all data TLB entries
 * - IIC trap if not privileged
 * - No flags affected
 * - Performance-critical for OS context switching
 * - Separate from PCTSB (program TLB)
 *
 * Common Use Cases:
 * - Operating system process context switch
 * - Domain switch in multi-domain programs
 * - After modifying page tables or capability tables
 * - Cache coherency maintenance after DMA transfers
 * - Memory-mapped I/O synchronization
 *
 * Typical Usage:
 *   Example 1: Process context switch
 *     DCTSB              ; Clear data TLB for new process
 *     PCTSB              ; Clear program TLB for new process
 *     ; Load new process context...
 *
 *   Example 2: After page table update
 *     ; Modify page table entries...
 *     DCTSB              ; Invalidate cached translations
 *
 *   Example 3: Domain switch
 *     ; Switch to new domain...
 *     DCTSB              ; Clear TLB for new capability tables
 *
 * Notes:
 * - Must be executed in privileged mode (PIA bit set)
 * - Separate from PCTSB (program TLB clear)
 * - Real hardware also flushes data cache write-back buffers
 * - Critical for memory management correctness
 * - Expensive operation (performance penalty after clearing)
 * - OS minimizes usage by selective TLB flushing when possible
 *
 * IMPLEMENTATION STATUS: FUNCTIONAL (Emulator no-op)
 *
 * Emulator behavior:
 * - Emulator does not implement TLB (always does full translation)
 * - DCTSB is a no-op (nothing to clear)
 * - Still validates privilege level and operand count
 * - Raises IIC trap if not privileged (for OS correctness)
 * - Functionally equivalent to real hardware (just slower)
 *
 * Why no TLB in emulator:
 * - Emulator runs on modern CPU with its own TLB for host memory
 * - TranslateVirtualAddress() function is already fast
 * - Adding TLB would complicate code with minimal benefit
 * - Correctness > Performance for emulation
 * - Full table lookup every time ensures accuracy
 *
 * Related Instructions:
 * - PCTSB: Clear program translation speedup buffer (instruction TLB)
 * - DCACHE: Clear data cache (cache control)
 * - ICACHE: Clear instruction cache (cache control)
 *
 * Reference: ND-500 Reference Manual, Chapter 16.24
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Dctsb.cs
 */
void nd500_instr_Dctsb(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (must be 0) */
    if (fi->operand_count != 0) {
        printf("[ERROR] DCTSB expects 0 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - DCTSB requires PIA bit set */
    if (!(cpu->ST1 & (1u << ND500_ST_BIT_PIA))) {
        printf("[ERROR] DCTSB requires privileged mode at PC=0x%08X\n", fi->address);
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

    /* EMULATOR NO-OP: Clear data translation speedup buffer
     *
     * In real hardware, this would:
     * 1. Invalidate all entries in the data TLB
     * 2. Flush write-back data cache (dirty lines to memory)
     * 3. Reset TLB associative lookup state
     * 4. Following data accesses perform full MMU translation
     *
     * In emulator:
     * - No TLB implementation (always full translation)
     * - No cache implementation (direct memory access)
     * - This instruction is effectively a no-op
     * - But still validates privilege level for OS correctness
     */

    {
        static int dbg = -1;
        if (dbg < 0) { const char* e = getenv("ND500X_TSBDBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (dbg)
            printf("[DCTSB] Data TLB clear (emulator no-op) at PC=0x%08X\n", fi->address);
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
     *   TOTAL: 3-5 memory accesses PER data access = EXTREMELY SLOW!
     *
     * Without a TLB, every load/store instruction would require 3-5 memory
     * lookups just to find where the data is! The ND-500 would be unusably slow.
     *
     * The TLB (Translation Lookaside Buffer) is a HARDWARE CACHE that stores
     * recent address translations:
     *   - Maps: (virtual_page) → (physical_page, permissions)
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
     *   Emulated ND-500 Memory → Host RAM → Host CPU TLB (already optimizing!)
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
     *   ✓ Ensures we catch bugs in page table setup
     *   ✓ Makes memory access behavior deterministic
     *   ✓ Simplifies debugging (no cache-related heisenbugs)
     *   ✓ Matches hardware behavior functionally (just slower)
     *
     * 4. WHY PRIVILEGE CHECKING IS STILL CRITICAL
     * ---------------------------------------------
     * Even though TLB clearing is a no-op, the privilege check is ESSENTIAL:
     *
     *   a) OS Code Testing: If OS code calls DCTSB without setting PIA first,
     *      it's a BUG that would fail on real hardware. The emulator MUST catch this!
     *
     *   b) Security Testing: If unprivileged code tries to execute DCTSB, it
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
