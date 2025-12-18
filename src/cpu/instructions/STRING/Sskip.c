#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SSKIP instruction - STRING class
 *
 * SSKIP - String skip
 *
 * Format: BY SSKIP <source/r/BY/I1=>, <test/r/BY>
 *
 * Assembly:
 *   BY SSKIP (string skip)  Hex 0xFDAE
 *
 * Operation:
 *   while not end of string and S(I1) = <test> do
 *     I1 + 1 -> I1
 *   enddo
 *
 * Description:
 *   Elements are skipped in the <source> string while elements
 *   equal to the <test> operand until a different element is found
 *   or the end of the string is reached.
 *
 * Terminating conditions:
 *   - outside source: K=0 Z=0 I1 unmodified, DR trap condition
 *   - non-matching element: K=0 Z=1 I1 := non-matching element
 *   - source empty: K=0 Z=0 I1 := next element, S=1
 *
 * Reference: ND-500 Reference Manual, Chapter 14.17
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Sskip.cs
 */
void nd500_instr_Sskip(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] SSKIP at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor address and test value */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t test_value = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Load string descriptor */
    Nd500StringDescriptor source_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, false, true, &source_desc)) {
        return;
    }

    /* Get starting index from I1 */
    uint32_t src_index = cpu->I[0];

    /* Skip matching elements */
    bool found_different = false;
    while (src_index < source_desc.element_count) {
        uint32_t addr = source_desc.base_address + src_index;
        uint8_t element = nd500_bus_read8(cpu->machine, addr);

        if (element != (test_value & 0xFF)) {
            found_different = true;
            break;
        }
        src_index++;
    }

    /* Update I1 register */
    cpu->I[0] = src_index;

    /* Set status flags */
    if (found_different) {
        nd500_set_flag(cpu, ND500_FLAG_Z);  /* Found non-matching */
        nd500_clear_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
        nd500_set_flag(cpu, ND500_FLAG_S);  /* End of string */
    }
    nd500_clear_flag(cpu, ND500_FLAG_K);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
/* Note: Sskip uses S flag for status, so can't use nd500_string_clear_unused_flags */
