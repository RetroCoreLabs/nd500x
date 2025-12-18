#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Zwip instruction - SYSTEM class
 *
 * ZWIP - Clear Written In Page Bit
 *
 * Format: BI ZWIP <bit no./r/W>
 *
 * Assembly:
 *   BI ZWIP (clear WIP bit)                     Hex 0xFE9C
 *
 * Operation: 0 -> specified WIP bit
 *
 * Description:
 *   Privileged instruction that clears the specified bit in the Written In
 *   Page table. This instruction is used by the swapper routines after a
 *   new page has been read from disk into physical memory. In hardware there
 *   are separate WIP tables for program and data. ZWIP will clear both tables.
 *   Consequently, an ND-500 system cannot have physically separate memory
 *   for program and data at the same physical addresses. This instruction is
 *   installation dependent; using it requires knowledge of the physical memory
 *   configuration.
 *
 *   EMULATOR NOTE: WIP tracking is not implemented in the emulator.
 *   This instruction is a no-op that succeeds without error.
 *
 * Trap conditions: Illegal instruction code (IIC), Illegal operand value (IOV)
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.18
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Zwip.cs
 */
void nd500_instr_Zwip(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 45-49) */
    if (fi->operand_count != 1) {
        printf("[ERROR] ZWIP at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - ZWIP requires privileged access (like C# lines 52-56) */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* Read operand (bit number / physical page number) - but we don't use it */
    /* since we don't track WIP state */
    /* uint32_t bit_number = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type); */

    /* EMULATOR NO-OP: Clear specified WIP bit */
    /* Reference: instructions.md Chapter 16.18 */
    /*
     * In real hardware, this clears the specified bit in both program
     * and data WIP tables.
     * In the emulator, we don't track dirty pages for swapping purposes,
     * so this is a no-op.
     */

    /* No status bits affected for this instruction */
}
