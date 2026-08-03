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
 *
 * OPERAND ENCODING - CORRECTED 2026-08-03 (same fix applied to RetroCore).
 * The instruction table marked RPHS/WPHS operand 0 as O_DIR (0x20000 in
 * operandTemplates), i.e. "four inline literal bytes, no address code". That
 * is WRONG. The operand is an ordinary operand.
 *
 * Proof from the SINTRAN swapper P-segment (SWAPPER-K01.PSEG, base 0o1000000000):
 *
 *   1000010525: 377 365 | 304 | 010 001 115 054   rphs <abs 0o1000246454>
 *   1000010534: 300 057                           go   $57
 *
 * 0o304 is the address code "32-bit absolute address follows". Proven by a
 * sibling instruction in the SAME routine that the disassembler already gets
 * right:
 *
 *   1000010477: 104 304 010 002 075 154           w test $1000436554
 *
 * 0x08023D6C in octal is exactly 0o1000436554. So RPHS is 2+1+4 = 7 bytes.
 * Read as a direct operand it is 6 bytes, so decoding resumed at 0o10533 -
 * inside the operand - and produced the swapper's "1 10533B" protect
 * violation. It also made the domain number 0xC408014D; with the address code
 * honoured the domain number is READ FROM MEMORY, which is what the manual's
 * "<domain number/r/W>" notation means.
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

        /* BREAK, not return - ALIGNED WITH RetroCore 2026-08-03. A trap part way
         * through must still leave I1/I2/I3 describing how far the move got,
         * because that is the only way the caller can resume it; returning here
         * abandoned the progress and left the registers describing the START of
         * a transfer that had already partly happened. The counters are advanced
         * only AFTER all three steps succeed, so a break leaves them pointing at
         * the byte that failed, which is what a restart needs.
         * ASSUMPTION, marked as such: no manual text was found stating what the
         * registers hold after an addressing trap mid-move. The two ports must
         * agree, and "resumable" is the reading the partial-move design implies. */
        uint32_t phys = nd500_mmu_translate_physical_segment(cpu, physical_segment,
                                                             segment_offset, 0);
        if (nd500_trap_occurred() || cpu->instr_aborted) break;

        uint8_t value = nd500_bus_read8(cpu->machine, phys);
        if (nd500_trap_occurred() || cpu->instr_aborted) break;

        nd500_write_memory_8(cpu, domain_address, value);
        if (nd500_trap_occurred() || cpu->instr_aborted) break;

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
