#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Rpgu instruction - SYSTEM class
 *
 * RPGU - Read Page Used Table
 *
 * Format: tn RPGU <bit or group no./r/W>
 *
 * Assembly:
 *   BIn RPGU (read PGU bit)                   Hex 0xFE88+(n-1)
 *   Hn  RPGU (read PGU group)                 Hex 0xFE8C+(n-1)
 *
 * Operation: specified PGU bit or group -> Rn
 *
 * Description:
 *   Privileged instruction that reads a bit or 16-bit group from the
 *   Page Used table into the specified register. The operand specifies
 *   the physical memory page number (BIn RPGU) or physical page number/16
 *   (Hn RPGU). A bit set in this table indicates that the page has been
 *   used in some instruction since the last time the bit was cleared.
 *
 *   EMULATOR NOTE: PGU tracking is not implemented in the emulator.
 *   This instruction returns 0 (pages not used) which is safe for
 *   OS swapping decisions (indicates pages can be safely swapped out).
 *
 * Trap conditions: Illegal instruction code (IIC), Illegal operand value (IOV)
 *
 * Data status bits: Z (result = 0)
 *
 * Reference: ND-500 Reference Manual, Chapter 16.20
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Rpgu.cs
 */
void nd500_instr_Rpgu(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 43-47) */
    if (fi->operand_count != 1) {
        printf("[ERROR] RPGU at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate target register */
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] RPGU at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - RPGU requires privileged access (like C# lines 50-54) */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* Read operand (page number or group number) - but we don't use it */
    /* since we don't track PGU state */
    /* uint32_t bit_or_group = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type); */

    /* EMULATOR: Return 0 (no pages used) */
    /* This is safe - tells OS all pages can be swapped out */
    uint32_t result = 0;

    /* Write result to target register (like C# line 91) */
    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Set Z flag since result is 0 */
    cpu->ST1 |= ND500_FLAG_Z;
}
