#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SCHPAR instruction - STRING class
 *
 * SCHPAR - String character parse
 *
 * Format: BY SCHPAR <source/r/BY/I1=>, <delimiters/r/BY>
 *
 * Assembly:
 *   BY SCHPAR (string character parse)  Hex 0xFDB5
 *
 * Operation:
 *   Parse string by skipping delimiter characters.
 *
 * Description:
 *   Scans the <source> string starting at I1, skipping any characters
 *   that match the delimiter set, until a non-delimiter is found.
 *
 * Reference: ND-500 Reference Manual, Chapter 14.20
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Schpar.cs
 */
void nd500_instr_Schpar(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] SCHPAR at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t delim_desc_addr = fi->operands[1].effective_address;

    /* Load string descriptors */
    Nd500StringDescriptor source_desc, delim_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, false, true, &source_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, delim_desc_addr, false, true, &delim_desc)) {
        return;
    }

    /* Get starting index from I1 */
    uint32_t src_index = cpu->I[0];

    /* Skip delimiter characters */
    bool found_non_delim = false;
    while (src_index < source_desc.element_count) {
        uint32_t addr = source_desc.base_address + src_index;
        uint8_t element = nd500_bus_read8(cpu->machine, addr);

        /* Check if element is a delimiter */
        bool is_delim = false;
        for (uint32_t i = 0; i < delim_desc.element_count; i++) {
            uint32_t delim_addr = delim_desc.base_address + i;
            uint8_t delim = nd500_bus_read8(cpu->machine, delim_addr);
            if (element == delim) {
                is_delim = true;
                break;
            }
        }

        if (!is_delim) {
            found_non_delim = true;
            break;
        }
        src_index++;
    }

    /* Update I1 register */
    cpu->I[0] = src_index;

    /* Set status flags */
    if (found_non_delim) {
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
