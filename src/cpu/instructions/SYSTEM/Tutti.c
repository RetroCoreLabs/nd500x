#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Tutti instruction - SYSTEM class
 *
 * TUTTI - Enable Process Switch
 *
 * Format: TUTTI
 *
 * Assembly:
 *   TUTTI (enable process switch)               Hex 0xFE01
 *
 * Operation: process switch is enabled
 *
 * Description:
 *   Privileged instruction that enables process switching. This instruction
 *   is used in conjunction with SOLO to implement critical sections and
 *   synchronization mechanisms. TUTTI re-enables process switching after
 *   a SOLO instruction has disabled it. Ignorable trap conditions are
 *   ignored in SOLO-TUTTI sequences regardless of enabling of these traps.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.2
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Tutti.cs
 */
void nd500_instr_Tutti(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Check privilege - TUTTI requires supervisor mode */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trapped - not privileged */
    }

    /* Validate operand count (like C# lines 40-45) */
    if (fi->operand_count != 0) {
        printf("[ERROR] TUTTI at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Clear Process Switch Disabled flag.
     * Reference: manual ch.16.2 - "allows normal interleaving of process
     * execution". Clearing PSD ends the SOLO region, which also stands down
     * the DT timeout measurement (it only runs while PSD is set) and the DE
     * check on the non-ignorable trap path. */
    cpu->ST1 &= ~ND500_FLAG_PSD;
    cpu->solo_start_icount = 0;

    /* No status bits affected for this instruction */
}
