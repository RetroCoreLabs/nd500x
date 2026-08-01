#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_page_bits.h"
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
 *   The single PGU table kept in nd500_page_bits.c stands in for both the
 *   hardware program and data tables, so clearing the bit once matches the
 *   "clears the specified bit in both tables" wording.
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

    /* Read operand: the physical page number whose PGU bit is to be cleared */
    uint32_t page = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;  /* The operand read faulted - commit nothing */
    }

    nd500_page_bits_count(ND500_PAGE_OP_ZPGU);
    nd500_page_bits_clear_bit(cpu->machine, ND500_PAGE_TABLE_PGU, page);

    /* Data status bits: Unaffected (ND-05.009.4 16.21) */
}
