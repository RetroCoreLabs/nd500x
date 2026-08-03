#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_mmu.h"
#include <stdio.h>

/**
 * Wphs instruction - SYSTEM class
 *
 * WPHS - Write to physical segment ('87 extension)
 *
 * Format: WPHS <domain number/r/W>          <- ONE operand
 *
 * Assembly:
 *   WPHS (write to physical segment)          Hex 0xFFF4   Octal 177764B
 *
 * Operation (ND-05.009.4 EN, section 16.32, quoted):
 *
 *   while I1 > 0 do
 *     S(<domain number>.I2) -> D(I4.I3)
 *     I3 + 1 -> I3
 *     I2 + 1 -> I2
 *     I1 - 1 -> I1
 *   enddo
 *
 *   I1 : Number of bytes to be moved.
 *   I2 : Logical address on the domain.
 *   I3 : Address on the physical segment.
 *   I4 : Physical segment number.
 *   Operand : domain number.
 *
 * Identical to RPHS (16.31) with the direction reversed: same register roles,
 * same "stop at I1 = 0 OR a page boundary on the physical segment", same Z
 * rule. See Rphs.c for the full quotation and the reasoning.
 *
 * Data status bits:
 *   no bytes left = 0                   : 1 -> Z
 *   page boundary and no bytes left < 0 : 0 -> Z
 *
 * KNOWN GAP, deliberately not faked: the domain operand is not honoured when
 * it differs from CED - see Rphs.c.
 *
 * Trap conditions: Addressing traps, Illegal instruction code (IIC)
 *
 * Reference: ND-500 Reference Manual, section 16.32
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Wphs.cs
 */
void nd500_instr_Wphs(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* ONE operand - the domain number. The decoder emits 1
     * (nd500_instructions.c: { 0xFFF4, "wphs", 1, ... }); this guard used to
     * demand 3, so it fired on every single execution. */
    if (fi->operand_count != 1) {
        printf("[ERROR] WPHS at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    uint32_t domain = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    uint32_t byte_count       = cpu->I[0];   /* I1 - bytes remaining      */
    uint32_t domain_address   = cpu->I[1];   /* I2 - logical addr, domain */
    uint32_t segment_offset   = cpu->I[2];   /* I3 - addr on the segment  */
    uint32_t physical_segment = cpu->I[3];   /* I4 - physical segment no. */

    if (domain != cpu->CED) {
        static int warned = 0;
        if (!warned++) {
            fprintf(stderr, "[WPHS] domain operand %u != CED %u at PC=0x%08X - the "
                            "domain-side access translates in CED (no cross-domain "
                            "translate exists in either port)\n",
                    domain, cpu->CED, fi->address);
        }
    }

    uint32_t moved = 0;
    while (byte_count > 0) {
        /* Page boundary on the PHYSICAL segment - see Rphs.c for why `moved > 0`. */
        if (moved > 0 && (segment_offset & (NBPG - 1)) == 0) break;

        uint8_t value = nd500_read_memory_8(cpu, domain_address);
        if (nd500_trap_occurred() || cpu->instr_aborted) return;

        uint32_t phys = nd500_mmu_translate_physical_segment(cpu, physical_segment,
                                                             segment_offset, 1);
        if (nd500_trap_occurred() || cpu->instr_aborted) return;

        nd500_bus_write8(cpu->machine, phys, value);
        if (nd500_trap_occurred() || cpu->instr_aborted) return;

        segment_offset++;
        domain_address++;
        byte_count--;
        moved++;
    }

    cpu->I[0] = byte_count;
    cpu->I[1] = domain_address;
    cpu->I[2] = segment_offset;

    if (byte_count == 0) cpu->ST1 |=  ND500_FLAG_Z;
    else                 cpu->ST1 &= ~ND500_FLAG_Z;
}
