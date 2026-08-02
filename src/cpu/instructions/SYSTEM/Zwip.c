#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_page_bits.h"
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
 *   The single WIP table kept in nd500_page_bits.c stands in for both the
 *   hardware program and data tables, so clearing the bit once matches the
 *   "ZWIP will clear both tables" wording.
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
    uint32_t page = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;  /* The operand read faulted - commit nothing */
    }

    nd500_page_bits_count(ND500_PAGE_OP_ZWIP);
    nd500_page_bits_clear_bit(cpu->machine, ND500_PAGE_TABLE_WIP, page);

    /* Data status bits: Unaffected (ND-05.009.4 16.18) */
}
