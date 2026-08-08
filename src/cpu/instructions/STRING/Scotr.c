#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SCOTR instruction - STRING class
 *
 * SCOTR - String compare translated
 *
 * Format: BY SCOTR <source-1/r/BY/I1=>, <source-2/r/BY/I2=>, <trans table/aa/BY>
 *
 * Assembly:
 *   BY SCOTR (string compare translated)  Hex 0xFDAD
 *
 * Operation:
 *   while not end of strings and tr(S(I1)) = tr(D(I2)) do
 *     I1 + 1 -> I1, I2 + 1 -> I2
 *   enddo
 *
 * Description:
 *   Translated bytes from the <source-1> string are compared with the
 *   corresponding translated bytes in the <source-2> string. The comparison
 *   continues until unequal bytes are found, or until the end of the
 *   <source-1> or <source-2> string is reached.
 *
 * Reference: ND-500 Reference Manual, Chapter 14.11
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Scotr.cs
 */
void nd500_instr_Scotr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 3) {
        printf("[ERROR] SCOTR at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses and translation table */
    uint32_t source1_desc_addr = fi->operands[0].effective_address;
    uint32_t source2_desc_addr = fi->operands[1].effective_address;
    uint32_t trans_table_addr = fi->operands[2].effective_address;

    /* Load string descriptors */
    Nd500StringDescriptor source1_desc, source2_desc;
    if (!nd500_load_string_descriptor(cpu, source1_desc_addr, false, true, &source1_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, source2_desc_addr, false, true, &source2_desc)) {
        return;
    }

    /* Get starting indices */
    uint32_t src1_index = cpu->I[0];
    uint32_t src2_index = cpu->I[1];

    /* Validate translation table address won't overflow 32-bit address space */
    /* Translation table is 256 bytes (one entry per possible byte value) */
    if (trans_table_addr > 0xFFFFFF00) {
        printf("[ERROR] SCOTR: Translation table address 0x%08X would overflow\n", trans_table_addr);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    /* Compare elements with translation */
    bool source1_outside = false;
    bool source2_outside = false;
    bool source1_greater = false;
    bool source1_smaller = false;

    while (!source1_outside && !source2_outside) {
        /* Check bounds */
        if (src1_index >= source1_desc.element_count) {
            source1_outside = true;
            break;
        }
        if (src2_index >= source2_desc.element_count) {
            source2_outside = true;
            break;
        }

        /* Read elements */
        uint32_t src1_addr = source1_desc.base_address + src1_index;
        uint32_t src2_addr = source2_desc.base_address + src2_index;
        uint8_t element1 = nd500_read_memory_8(cpu, src1_addr);
        uint8_t element2 = nd500_read_memory_8(cpu, src2_addr);

        /* Translate elements via 256-byte translation table */
        uint8_t translated1 = nd500_read_memory_8(cpu, trans_table_addr + element1);
        uint8_t translated2 = nd500_read_memory_8(cpu, trans_table_addr + element2);

        /* Compare translated elements */
        if (translated1 != translated2) {
            if (translated1 > translated2) {
                source1_greater = true;
            } else {
                source1_smaller = true;
            }
            break;
        }

        /* Elements match, advance */
        src1_index++;
        src2_index++;

        if (src1_index >= source1_desc.element_count)
            source1_outside = true;
        if (src2_index >= source2_desc.element_count)
            source2_outside = true;
    }

    /* Update index registers */
    cpu->I[0] = src1_index;
    cpu->I[1] = src2_index;

    /* Set status flags based on termination condition */
    if (source1_outside && source2_outside) {
        /* Both exhausted - exact match */
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_set_flag(cpu, ND500_FLAG_Z);
        nd500_clear_flag(cpu, ND500_FLAG_S);
    } else if (source1_greater) {
        nd500_set_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
        nd500_clear_flag(cpu, ND500_FLAG_S);
    } else if (source1_smaller) {
        nd500_set_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else if (source1_outside && !source2_outside) {
        /* Source2 longer */
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else if (!source1_outside && source2_outside) {
        /* Source1 longer */
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
