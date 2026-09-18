/*
 * Smvtr.c - ND-500 SMVTR instruction (STRING class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SMVTR instruction - STRING class
 *
 * SMVTR - String move translated
 *
 * Format: BY SMVTR <source/r/BY/I1=>, <dest/w/BY/I2=>, <trans table/aa/BY>
 *
 * Assembly:
 *   BY SMVTR (byte string move translated)  Hex 0xFD74
 *
 * Operation:
 *   while not end of strings do
 *     tr(S(I1)) -> D(I2), I1 + 1 -> I1, I2 + 1 -> I2
 *   enddo
 *
 * Description:
 *   Bytes from the <source> operand are translated via a translation table
 *   and moved to <dest> until <source> is empty or <dest> is full.
 *
 * Terminating conditions:
 *   - source empty: K=0
 *   - dest full: K=1
 *
 * Reference: ND-500 Reference Manual, Chapter 14.5
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Smvtr.cs
 */
void nd500_instr_Smvtr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 3) {
        printf("[ERROR] SMVTR at PC=0x%08X: Expected 3 operands, got %u\n",
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
        printf("[ERROR] SMVTR: Translation table address 0x%08X would overflow\n", trans_table_addr);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    /* Copy with translation until source empty or dest full */
    while (src_index < source_desc.element_count && dest_index < dest_desc.element_count) {
        /* Read source element */
        uint32_t src_addr = source_desc.base_address + src_index;
        uint8_t element = nd500_read_memory_8(cpu, src_addr);

        /* Translate through 256-byte table: element (0-255) -> translated byte */
        uint8_t translated = nd500_read_memory_8(cpu, trans_table_addr + element);

        /* Write to destination */
        uint32_t dest_addr = dest_desc.base_address + dest_index;
        nd500_write_memory_8(cpu, dest_addr, translated);
        /* Fault mid-instruction: leave the index registers naming only the
         * COMPLETED elements so the restart redoes this one (see Smvun). */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            cpu->I[0] = src_index; cpu->I[1] = dest_index;
            return;
        }

        src_index++;
        dest_index++;
    }

    /* Update index registers */
    cpu->I[0] = src_index;
    cpu->I[1] = dest_index;

    /* Set status flags */
    if (src_index >= source_desc.element_count) {
        /* Source empty */
        nd500_clear_flag(cpu, ND500_FLAG_K);
    } else if (dest_index >= dest_desc.element_count) {
        /* Dest full */
        nd500_set_flag(cpu, ND500_FLAG_K);
    }

    nd500_clear_flag(cpu, ND500_FLAG_Z);
    nd500_string_clear_unused_flags(cpu);  /* Clears S, C, O */
}
