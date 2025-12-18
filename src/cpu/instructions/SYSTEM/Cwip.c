#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Cwip instruction - SYSTEM class
 *
 * CWIP - Clear Written In Page Table
 *
 * Format: CWIP
 *
 * Assembly:
 *   CWIP (clear WIP table)                   Hex 0xFF1B
 *
 * Operation: 0 -> entire WIP table
 *
 * Description:
 *   Privileged instruction that clears the entire Written In Page table.
 *   The WIP table tracks which pages have been written to and must be
 *   written back to disk before being replaced. This instruction is used
 *   by the swapper routines to reset the WIP tracking state.
 *
 *   EMULATOR NOTE: WIP tracking is not implemented in the emulator as it's
 *   primarily used by the OS page swapping system. This instruction is a
 *   no-op that succeeds without error.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.19
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Cwip.cs
 */
void nd500_instr_Cwip(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 41-45) */
    if (fi->operand_count != 0) {
        printf("[ERROR] CWIP at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - CWIP requires privileged access (like C# lines 48-52) */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* EMULATOR NO-OP: Clear entire Written In Page table */
    /* Reference: instructions.md Chapter 16.19 */
    /*
     * In real hardware, this clears all bits in the WIP table.
     * In the emulator, we don't track dirty pages for swapping purposes,
     * so this is a no-op.
     */

    /* No status bits affected for this instruction */
}
