#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_page_bits.h"
#include <stdio.h>

/**
 * Rwip instruction - SYSTEM class
 *
 * RWIP - Read Written In Page Table
 *
 * Format: tn RWIP <bit or group no./r/W>
 *
 * Assembly:
 *   BIn RWIP (read WIP bit)                  Hex 0xFE94+(n-1)
 *   Hn  RWIP (read WIP group)                Hex 0xFE98+(n-1)
 *
 * Operation: specified WIP bit or group -> Rn
 *
 * Description:
 *   Privileged instruction that reads a bit or 16-bit group from the
 *   Written In Page table into the specified register. The operand specifies
 *   the physical memory page number (BIn RWIP) or physical page number/16
 *   (Hn RWIP). A bit set in this table indicates that the page has been
 *   written into and must be written back to disk before being replaced; the
 *   bit is set automatically by hardware and is used by the swapper routines.
 *
 *   In hardware there are separate WIP tables for program and data and RWIP
 *   returns the logical OR of the two, so the single table kept in
 *   nd500_page_bits.c is behaviourally identical.
 *
 *   Only the lower 25 bits of the page number are significant, and reading
 *   bits representing non-existing memory gives a zero result. Both rules
 *   live in nd500_page_bits_read_bit/_group.
 *
 *   This used to return a hardwired 0, which loses data rather than being
 *   safe: NDIX's dirty(pte) (kernel/MASTER/machine/pte.h:93) is
 *   `_rwip(pfnum) || pg_m`, so a page the guest wrote through the MMU without
 *   the kernel's own pg_m being set looked clean and was dropped instead of
 *   written back.
 *
 * Trap conditions: Addressing traps, Illegal instruction code (IIC)
 *
 * Data status bits: bit or bit group = 0 -> Z
 *
 * Reference: ND-500 Reference Manual, Chapter 16.17
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Rwip.cs
 */

/* Hn RWIP (group form) is 0xFE98..0xFE9B; BIn RWIP (bit form) is 0xFE94..0xFE97. */
#define RWIP_GROUP_FORM_FIRST 0xFE98u

void nd500_instr_Rwip(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 43-47) */
    if (fi->operand_count != 1) {
        printf("[ERROR] RWIP at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate target register */
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] RWIP at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - RWIP requires privileged access (like C# lines 50-54) */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* The operand is a PAGE NUMBER and must be read as a word.
     *
     * The "BI" in "BIn RPGU" names the TABLE being addressed - a bit in the
     * PGU/WIP table - not the width of the operand that selects it.
     * ND-05.009.4 16.20 is explicit: "The operand specifies the physical memory
     * page number (BIn RPGU) or physical page number/16 (Hn RPGU)", and 16.17
     * adds "Only the lower 25 bits of the bit number are significant" - a
     * 25-bit-significant number cannot travel in a bit.
     *
     * The decoder types the BI prefix as ND500_DTYPE_BIT (1 bit), which is
     * correct for genuine bit-addressing instructions but wrong here: reading
     * fi->data_type gave a 1-bit value, so under a real NDIX paging load every
     * query arrived as page 0 while 1482 pages were marked in the range
     * 17..3071, and the pageout clock hand took every page it inspected.
     * NDIX passes the number as a plain 32-bit frame argument
     * ("bi1 rpgu b.20", machine/locore.c:1218). */
    uint32_t operand = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;  /* The operand read faulted - commit nothing */
    }

    nd500_page_bits_count(ND500_PAGE_OP_RWIP);

    uint32_t result;
    if (fi->opcode >= RWIP_GROUP_FORM_FIRST) {
        result = nd500_page_bits_read_group(cpu->machine, ND500_PAGE_TABLE_WIP, operand);
    } else {
        result = nd500_page_bits_read_bit(cpu->machine, ND500_PAGE_TABLE_WIP, operand);
    }

    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Data status: bit or bit group = 0 -> Z */
    if (result == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~(uint32_t)ND500_FLAG_Z;
    }
}
