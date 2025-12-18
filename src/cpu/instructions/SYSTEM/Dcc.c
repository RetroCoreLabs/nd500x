#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Dcc instruction - SYSTEM class
 *
 * DCC - Data Cache Clear
 *
 * Format: DCC
 *
 * Assembly:
 *   DCC (data cache clear)                    Hex 0xFF15
 *
 * Operation: Clear data cache
 *
 * Description:
 *   Privileged instruction that clears the data cache. This instruction
 *   is used to invalidate all entries in the data cache, forcing all
 *   subsequent data accesses to go to main memory. This is typically
 *   used during system initialization or when cache coherency needs to
 *   be maintained in multiprocessor systems.
 *
 *   EMULATOR NOTE: This is a no-op in the emulator since we don't have a
 *   separate data cache. All data accesses go directly to memory.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.10
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Dcc.cs
 */
void nd500_instr_Dcc(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 40-45) */
    if (fi->operand_count != 0) {
        printf("[ERROR] DCC at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - DCC is a privileged instruction (like C# lines 48-53) */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* EMULATOR NO-OP: Data cache clear is not implemented in emulator */
    /* In hardware, this would clear the data cache to force subsequent */
    /* data accesses to go to main memory for cache coherency */

    /* No status bits affected for this instruction */
}
