#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SMVTU instruction - STRING class
 *
 * SMVTU - String move translated until
 *
 * Format: BY SMVTU <source/r/BY/I1=>, <dest/w/BY/I2=>, <trans table/aa/BY>
 *
 * Assembly:
 *   BY SMVTU (byte string move translated until)  Hex 0xFD75
 *
 * Operation:
 *   while not end of strings and tr(S(I1)) <> ASCII "escape" do
 *     if tr(S(I1)) <> zero then
 *       tr(S(I1)) -> D(I2), I2 + 1 -> I2
 *     endif
 *     I1 + 1 -> I1
 *   enddo
 *
 * Description:
 *   Bytes from <source> are translated via translation table. Translated
 *   bytes are moved to <dest> if non-zero. Stops on ASCII escape (0x1B),
 *   source empty, or dest full. Escape character is not moved.
 *
 * Terminating conditions:
 *   - escape found: K=0 Z=1
 *   - source empty: K=0 Z=0
 *   - dest full: K=1 Z=0
 *
 * Reference: ND-500 Reference Manual, Chapter 14.6
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Smvtu.cs
 */
void nd500_instr_Smvtu(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 3) {
        printf("[ERROR] SMVTU at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses and translation table */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t dest_desc_addr = fi->operands[1].effective_address;
    uint32_t trans_table_addr = fi->operands[2].effective_address;

    /* Load string descriptors */
    Nd500StringDescriptor source_desc, dest_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, false, true, &source_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, false, true, &dest_desc)) {
        return;
    }

    /* Get starting indices */
    uint32_t src_index = cpu->I[0];
    uint32_t dest_index = cpu->I[1];

    /* Validate translation table address won't overflow 32-bit address space */
    /* Translation table is 256 bytes (one entry per possible byte value) */
    if (trans_table_addr > 0xFFFFFF00) {
        printf("[ERROR] SMVTU: Translation table address 0x%08X would overflow\n", trans_table_addr);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    /* ASCII escape character */
    const uint8_t ESCAPE_CHAR = 0x1B;

    /* Copy with translation until escape, source empty, or dest full */
    bool escape_found = false;
    while (src_index < source_desc.element_count && dest_index < dest_desc.element_count) {
        /* Read source element */
        uint32_t src_addr = source_desc.base_address + src_index;
        uint8_t element = nd500_read_memory_8(cpu, src_addr);

        /* Translate through 256-byte table: element (0-255) -> translated byte */
        uint8_t translated = nd500_read_memory_8(cpu, trans_table_addr + element);

        /* Check for escape character */
        if (translated == ESCAPE_CHAR) {
            escape_found = true;
            break;
        }

        /* Only move non-zero translated elements */
        if (translated != 0) {
            uint32_t dest_addr = dest_desc.base_address + dest_index;
            nd500_write_memory_8(cpu, dest_addr, translated);
            /* Fault mid-instruction: indices must name only COMPLETED
             * elements so the restart redoes this one (see Smvun). */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                cpu->I[0] = src_index; cpu->I[1] = dest_index;
                return;
            }
            dest_index++;
        }

        /* Always advance source */
        src_index++;
    }

    /* Update index registers */
    cpu->I[0] = src_index;
    cpu->I[1] = dest_index;

    /* Set status flags */
    if (escape_found) {
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else if (src_index >= source_desc.element_count) {
        /* Source empty */
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    } else if (dest_index >= dest_desc.element_count) {
        /* Dest full */
        nd500_set_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    nd500_string_clear_unused_flags(cpu);  /* Clears S, C, O */
}
