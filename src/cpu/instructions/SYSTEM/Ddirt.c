#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Ddirt instruction - SYSTEM class
 *
 * DDIRT - Dump Dirty (Cache Writeback)
 *
 * Format: DDIRT
 *
 * Assembly:
 *   DDIRT (dump dirty)                        Hex 0xFE1E
 *
 * Operation: Write dirty cache lines back to memory
 *
 * Description:
 *   Privileged instruction that dumps (writes back) dirty cache lines
 *   to main memory. This instruction is used to force all modified (dirty)
 *   cache entries to be written back to main memory. This is typically
 *   used before cache invalidation or when ensuring data consistency in
 *   multiprocessor systems. This is an '87 extension instruction.
 *
 *   EMULATOR NOTE: This is a no-op in the emulator since we don't have a
 *   cache system. All writes go directly to memory without buffering.
 *   If no cache is present, instruction has no effect.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.11
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Ddirt.cs
 */
void nd500_instr_Ddirt(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* NOT PRIVILEGED - guard removed 2026-08-09.
     *
     * Manual ch.16.11 DDIRT - Dump 'Dirty': the entry has NO "Privileged instruction" line in its
     * Description, and gives "Trap conditions: None". That line is where the
     * manual records privilege - 15.17 CLINIT and 16.13 DMON both carry it and
     * both still say "Trap Conditions: None", so the trap list is not the
     * marker.
     *
     * Microcode NOT checked for this one - no DDIRT label was found in
     * MICRO-5800-B30.LABE, so this rests on the manual alone plus the fact
     * that its two siblings DCC and PCC have no privilege test in the
     * control store.
     *
     * Found by the sweep prompted by the same bug in TUTTI: a guard that came
     * from the C# port rather than from any source.
     */

    /* Validate operand count (like C# lines 40-45) */
    if (fi->operand_count != 0) {
        printf("[ERROR] DDIRT at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* EMULATOR NO-OP: Dump dirty cache lines */
    /* Reference: instructions.md Chapter 16.11 */
    /*
     * In real hardware, this writes all modified (dirty) cache lines back to memory.
     * In the emulator, we don't have a cache system, so this is a no-op.
     *
     * Note: If no cache is present (like in emulator), instruction has no effect.
     */

    /* No status bits affected for this instruction */
}
