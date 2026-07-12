#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SCOPT instruction - STRING class
 *
 * SCOPT - String copy translate
 *
 * Format: BY SCOPT <source/r/BY/I1=>, <dest/w/BY/I2=>, <table/r/BY>, <stop/r/BY>
 *
 * Assembly:
 *   BY SCOPT (string copy translate)  Hex 0xFDBF
 *
 * Operation:
 *   Copy elements with translation until stop character is found.
 *
 * Description:
 *   Copies elements from <source> to <dest>, translating through <table>,
 *   until the <stop> character is found in the translated output.
 *
 * Reference: ND-500 Reference Manual, Chapter 14.23
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Scopt.cs
 */
void nd500_instr_Scopt(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 4) {
        printf("[ERROR] SCOPT at PC=0x%08X: Expected 4 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses and stop value */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t dest_desc_addr = fi->operands[1].effective_address;
    uint32_t table_desc_addr = fi->operands[2].effective_address;
    uint32_t stop_value = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[3], fi->data_type);

    /* Load string descriptors */
    Nd500StringDescriptor source_desc, dest_desc, table_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, false, true, &source_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, false, true, &dest_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, table_desc_addr, false, true, &table_desc)) {
        return;
    }

    /* Get starting indices */
    uint32_t src_index = cpu->I[0];
    uint32_t dest_index = cpu->I[1];

    /* Copy with translation until stop */
    bool found_stop = false;
    while (src_index < source_desc.element_count && dest_index < dest_desc.element_count) {
        uint32_t src_addr = source_desc.base_address + src_index;
        uint8_t element = nd500_read_memory_8(cpu, src_addr);

        /* Translate through table */
        uint8_t translated;
        if (element < table_desc.element_count) {
            uint32_t table_addr = table_desc.base_address + element;
            translated = nd500_read_memory_8(cpu, table_addr);
        } else {
            translated = element;
        }

        /* Check for stop character */
        if (translated == (stop_value & 0xFF)) {
            found_stop = true;
            break;
        }

        /* Write to destination */
        uint32_t dest_addr = dest_desc.base_address + dest_index;
        nd500_write_memory_8(cpu, dest_addr, translated);

        src_index++;
        dest_index++;
    }

    /* Update index registers */
    cpu->I[0] = src_index;
    cpu->I[1] = dest_index;

    /* Set status flags */
    if (found_stop) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }
    if (dest_index >= dest_desc.element_count) {
        nd500_set_flag(cpu, ND500_FLAG_K);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_K);
    }
    nd500_clear_flag(cpu, ND500_FLAG_S);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
