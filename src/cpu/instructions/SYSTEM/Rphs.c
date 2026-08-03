#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_mmu.h"
#include <stdio.h>

/**
 * Rphs instruction - SYSTEM class
 *
 * RPHS - Read from physical segment ('87 extension)
 *
 * Format: RPHS <domain number/r/W>          <- ONE operand
 *
 * Assembly:
 *   RPHS (read from physical segment)         Hex 0xFFF5   Octal 177765B
 *
 * Operation (ND-05.009.4 EN, section 16.31, quoted):
 *
 *   while I1 > 0 do
 *     S([I4,I3) -> D(<domain number>.I2)
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
 * "The copy operation is continued until the number of bytes left is equal to
 *  0 (I1 = 0) or a page boundary is reached on the physical segment."
 *
 * "The physical segment number is used together with the physical segment
 *  table pointer to find the physical page number of the wanted data page or
 *  of the corresponding index page."
 *
 * So this is a PARTIAL move that the caller loops on, and the registers are
 * the interface: I1 counts down, I2 and I3 advance, and Z distinguishes
 * "finished" from "stopped at a page boundary".
 *
 * Data status bits:
 *   no bytes left = 0                   : 1 -> Z
 *   page boundary and no bytes left < 0 : 0 -> Z
 *
 * KNOWN GAP, deliberately not faked: the domain operand is not honoured when
 * it differs from CED. The domain-side access uses the ordinary read/write
 * path, which translates in the CURRENT domain, and neither this port nor
 * RetroCore has a "translate in domain N" entry point. Every RPHS observed so
 * far targets CED. A cross-domain one is a real finding - it is logged, not
 * invented.
 *
 * Trap conditions: Addressing traps, Illegal instruction code (IIC)
 *
 * Reference: ND-500 Reference Manual, section 16.31
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Rphs.cs
 */
void nd500_instr_Rphs(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* ONE operand - the domain number. The decoder emits 1
     * (nd500_instructions.c: { 0xFFF5, "rphs", 1, ... }); this guard used to
     * demand 3, so it fired on every single execution and the instruction
     * could never run at all. */
    if (fi->operand_count != 1) {
        printf("[ERROR] RPHS at PC=0x%08X: Expected 1 operand, got %u\n",
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
            fprintf(stderr, "[RPHS] domain operand %u != CED %u at PC=0x%08X - the "
                            "domain-side access translates in CED (no cross-domain "
                            "translate exists in either port)\n",
                    domain, cpu->CED, fi->address);
        }
    }

    uint32_t moved = 0;
    while (byte_count > 0) {
        /* Stop at a page boundary ON THE PHYSICAL SEGMENT. `moved > 0` so that
         * an I3 which already sits exactly on a boundary still transfers its
         * page instead of returning zero bytes forever. */
        if (moved > 0 && (segment_offset & (NBPG - 1)) == 0) break;

        uint32_t phys = nd500_mmu_translate_physical_segment(cpu, physical_segment,
                                                             segment_offset, 0);
        if (nd500_trap_occurred() || cpu->instr_aborted) return;

        uint8_t value = nd500_bus_read8(cpu->machine, phys);
        if (nd500_trap_occurred() || cpu->instr_aborted) return;

        nd500_write_memory_8(cpu, domain_address, value);
        if (nd500_trap_occurred() || cpu->instr_aborted) return;

        segment_offset++;
        domain_address++;
        byte_count--;
        moved++;
    }

    /* The registers ARE the interface - a caller loops on them. I1 is what is
     * LEFT (not 0), I3 is where the segment side stopped (it was never updated
     * at all before), and Z says whether the move completed. */
    cpu->I[0] = byte_count;
    cpu->I[1] = domain_address;
    cpu->I[2] = segment_offset;

    if (byte_count == 0) cpu->ST1 |=  ND500_FLAG_Z;
    else                 cpu->ST1 &= ~ND500_FLAG_Z;
}
