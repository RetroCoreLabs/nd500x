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
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Pcc.cs
 */
void nd500_instr_Pcc(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 0) {
        printf("[ERROR] PCC at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - PCC is a privileged instruction */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* EMULATOR NO-OP: Program cache clear is not implemented in emulator */
    /* In hardware, this would clear the instruction cache to force subsequent */
    /* instruction fetches to go to main memory for cache coherency */

    /* No status bits affected for this instruction */
}
