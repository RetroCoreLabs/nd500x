#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Cpgu instruction - SYSTEM class
 *
 * CPGU - Clear Page Used Table
 *
 * Format: CPGU
 *
 * Assembly:
 *   CPGU (clear PGU table)                   Hex 0xFF1A
 *
 * Operation: 0 -> entire PGU table
 *
 * Description:
 *   Privileged instruction that clears the entire Page Used table.
 *   The PGU table tracks which pages have been used in some instruction
 *   since the last time the bit was cleared. This instruction is used
 *   by the swapping routines to reset the PGU tracking state.
 *
 *   EMULATOR NOTE: PGU tracking is not implemented in the emulator as it's
 *   primarily used by the OS page swapping system. This instruction is a
 *   no-op that succeeds without error.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.22
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Cpgu.cs
 */
void nd500_instr_Cpgu(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 41-45) */
    if (fi->operand_count != 0) {
        printf("[ERROR] CPGU at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - CPGU requires privileged access (like C# lines 48-52) */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* EMULATOR NO-OP: Clear entire Page Used table */
    /* Reference: instructions.md Chapter 16.22 */
    /*
     * In real hardware, this clears all bits in the PGU table.
     * In the emulator, we don't track page usage for swapping purposes,
     * so this is a no-op.
     */

    /* No status bits affected for this instruction */
}
