#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Pcc instruction - SYSTEM class
 *
 * PCC - Program Cache Clear
 *
 * Format: PCC
 *
 * Assembly:
 *   PCC (program cache clear)             Hex 0xFF14
 *
 * Operation: Clear program cache
 *
 * Description:
 *   Privileged instruction that clears the program (instruction) cache.
 *   This instruction is used to invalidate all entries in the instruction
 *   cache, forcing all subsequent instruction fetches to go to main memory.
 *   This is typically used during system initialization, after modifying
 *   code in memory, or when cache coherency needs to be maintained in
 *   multiprocessor systems.
 *
 *   EMULATOR NOTE: This is a no-op in the emulator since we don't have a
 *   separate instruction cache. All instruction fetches go directly to
 *   memory.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Pcc.cs
 */
void nd500_instr_Pcc(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 0) {
        printf("[ERROR] PCC at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* NOT PRIVILEGED - guard removed 2026-08-09.
     *
     * Manual ch.16.12 Program cache clear: the entry has NO "Privileged instruction" line in its
     * Description, and gives "Trap conditions: None". That line is where the
     * manual records privilege - 15.17 CLINIT and 16.13 DMON both carry it and
     * both still say "Trap Conditions: None", so the trap list is not the
     * marker.
     *
     * The ND-5000 control store agrees: PCC (000717) jumps to PCC_IC (012273)
     * and on to CLR_IC, with no PIA test anywhere in the path.
     *
     * Found by the sweep prompted by the same bug in TUTTI: a guard that came
     * from the C# port rather than from any source. 
     */

    /* EMULATOR NO-OP: Program cache clear is not implemented in emulator */
    /* In hardware, this would clear the instruction cache to force subsequent */
    /* instruction fetches to go to main memory for cache coherency */

    /* No status bits affected for this instruction */
}
