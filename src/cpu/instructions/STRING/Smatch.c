#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SMATCH instruction - STRING class
 *
 * SMATCH - String pattern match
 *
 * Format: BY SMATCH <source/r/BY/I1=>, <pattern/r/BY/I3=>
 *
 * Assembly:
 *   BY SMATCH (string pattern match)  Hex 0xFDB3
 *
 * Operation:
 *   Search for pattern in source string starting at I1.
 *
 * Description:
 *   Searches for the <pattern> string within the <source> string
 *   starting at index I1. If found, I1 is updated to the match position.
 *
 * Terminating conditions:
 *   - pattern found: Z=1, I1 := match position
 *   - pattern not found: Z=0, S=1, I1 := end of source
 *
 * Reference: ND-500 Reference Manual, Chapter 14.19
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Smatch.cs
 */
void nd500_instr_Smatch(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] SMATCH at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t pattern_desc_addr = fi->operands[1].effective_address;

    /* Load string descriptors */
    Nd500StringDescriptor source_desc, pattern_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, false, true, &source_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, pattern_desc_addr, false, true, &pattern_desc)) {
        return;
    }

    /* Get starting index from I1 */
    uint32_t src_index = cpu->I[0];
    uint32_t pattern_len = pattern_desc.element_count;

    /* Search for pattern in source */
    bool found = false;
    while (src_index + pattern_len <= source_desc.element_count) {
        bool match = true;
        for (uint32_t i = 0; i < pattern_len; i++) {
            uint32_t src_addr = source_desc.base_address + src_index + i;
            uint32_t pat_addr = pattern_desc.base_address + i;
            uint8_t src_elem = nd500_read_memory_8(cpu, src_addr);
            uint8_t pat_elem = nd500_read_memory_8(cpu, pat_addr);
            if (src_elem != pat_elem) {
                match = false;
                break;
            }
        }
        if (match) {
            found = true;
            break;
        }
        src_index++;
    }

    /* Update I1 register */
    if (!found) {
        src_index = source_desc.element_count;
    }
    cpu->I[0] = src_index;

    /* Set status flags */
    if (found) {
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
