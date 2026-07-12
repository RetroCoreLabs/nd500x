#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SSPAN instruction - STRING class
 *
 * SSPAN - String span
 *
 * Format: BY SSPAN <source/r/BY/I1=>, <set/r/BY/I3=>, <test/r/BY>
 *
 * Assembly:
 *   BY SSPAN (string span)  Hex 0xFDB2
 *
 * Operation:
 *   while not end of source and S(I1) in <set> do
 *     I1 + 1 -> I1
 *   enddo
 *
 * Description:
 *   Elements are skipped in the <source> string while they are
 *   members of the character set specified by <set>.
 *
 * Terminating conditions:
 *   - outside source: K=0 Z=0 I1 unmodified, DR trap condition
 *   - element not in set: K=0 Z=1 I1 := element not in set
 *   - source empty: K=0 Z=0 I1 := next element, S=1
 *
 * Reference: ND-500 Reference Manual, Chapter 14.18
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Sspan.cs
 */
void nd500_instr_Sspan(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] SSPAN at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t set_desc_addr = fi->operands[1].effective_address;

    /* Load string descriptors */
    Nd500StringDescriptor source_desc, set_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, false, true, &source_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, set_desc_addr, false, true, &set_desc)) {
        return;
    }

    /* Get starting index from I1 */
    uint32_t src_index = cpu->I[0];

    /* Span while elements are in set */
    bool found_not_in_set = false;
    while (src_index < source_desc.element_count) {
        uint32_t addr = source_desc.base_address + src_index;
        uint8_t element = nd500_read_memory_8(cpu, addr);

        /* Check if element is in set */
        bool in_set = false;
        for (uint32_t i = 0; i < set_desc.element_count; i++) {
            uint32_t set_addr = set_desc.base_address + i;
            uint8_t set_elem = nd500_read_memory_8(cpu, set_addr);
            if (element == set_elem) {
                in_set = true;
                break;
            }
        }

        if (!in_set) {
            found_not_in_set = true;
            break;
        }
        src_index++;
    }

    /* Update I1 register */
    cpu->I[0] = src_index;

    /* Set status flags */
    if (found_not_in_set) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
        nd500_clear_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
        nd500_set_flag(cpu, ND500_FLAG_S);
    }
    nd500_clear_flag(cpu, ND500_FLAG_K);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
/* Note: Sspan uses S flag for status, so can't use nd500_string_clear_unused_flags */
