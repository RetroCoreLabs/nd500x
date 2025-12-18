#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Zpgu instruction - SYSTEM class
 *
 * ZPGU - Clear Page Used Bit
 *
 * Format: BI ZPGU <bit no./r/W>
 *
 * Assembly:
 *   BI ZPGU (clear PGU bit)                    Hex 0xFE90
 *
 * Operation: 0 -> specified PGU bit
 *
 * Description:
 *   Privileged instruction that clears the specified bit in the Page Used
 *   table. This instruction is used by the swapper routines after a new
 *   page has been read from disk into physical memory. Separate program
 *   and data PGU tables exist; ZPGU clears the specified bit in both tables.
 *   This instruction is installation dependent; using it requires knowledge
 *   of the physical memory configuration.
 *
 *   EMULATOR NOTE: PGU tracking is not implemented in the emulator.
 *   This instruction is a no-op that succeeds without error.
 *
 * Trap conditions: Illegal instruction code (IIC), Illegal operand value (IOV)
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.21
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Zpgu.cs
 */
void nd500_instr_Zpgu(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 43-47) */
    if (fi->operand_count != 1) {
        printf("[ERROR] ZPGU at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - ZPGU requires privileged access (like C# lines 50-54) */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* Read operand (bit number / physical page number) - but we don't use it */
    /* since we don't track PGU state */
    /* uint32_t bit_number = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type); */

    /* EMULATOR NO-OP: Clear specified PGU bit */
    /* Reference: instructions.md Chapter 16.21 */
    /*
     * In real hardware, this clears the specified bit in both program
     * and data PGU tables.
     * In the emulator, we don't track page usage for swapping purposes,
     * so this is a no-op.
     */

    /* No status bits affected for this instruction */
}
