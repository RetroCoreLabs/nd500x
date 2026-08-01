#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_page_bits.h"
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
 *   used in some instruction since the last time the bit was cleared; the
 *   bit is set automatically by hardware and is used by the swapping
 *   routines.
 *
 *   In hardware there are separate PGU tables for program and data and RPGU
 *   returns the logical OR of the two, so the single table kept in
 *   nd500_page_bits.c is behaviourally identical.
 *
 *   Only the lower 25 bits of the page number are significant, and reading
 *   bits representing non-existing memory gives a zero result. Both rules
 *   live in nd500_page_bits_read_bit/_group.
 *
 *   This used to return a hardwired 0. That is NOT the safe answer it was
 *   documented to be: NDIX's pageout clock hand
 *   (kernel/MASTER/sys/vm_page.c:547) reclaims any page RPGU reports as 0, so
 *   an always-zero table made the hand take every page it inspected.
 *
 * Trap conditions: Illegal instruction code (IIC), Illegal operand value (IOV)
 *
 * Data status bits: bit or bit group = 0 -> Z
 *
 * Reference: ND-500 Reference Manual, Chapter 16.20
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Rpgu.cs
 */

/* Hn RPGU (group form) is 0xFE8C..0xFE8F; BIn RPGU (bit form) is 0xFE88..0xFE8B. */
#define RPGU_GROUP_FORM_FIRST 0xFE8Cu

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

    /* Read operand: physical page number (bit form) or page number/16 (group form) */
    uint32_t operand = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;  /* The operand read faulted - commit nothing */
    }

    uint32_t result;
    if (fi->opcode >= RPGU_GROUP_FORM_FIRST) {
        result = nd500_page_bits_read_group(cpu->machine, ND500_PAGE_TABLE_PGU, operand);
    } else {
        result = nd500_page_bits_read_bit(cpu->machine, ND500_PAGE_TABLE_PGU, operand);
    }

    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Data status: bit or bit group = 0 -> Z */
    if (result == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~(uint32_t)ND500_FLAG_Z;
    }
}
