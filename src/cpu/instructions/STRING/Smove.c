/*
 * Smove.c - ND-500 SMOVE instruction (STRING class)
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
 * SMOVE instruction - STRING class
 *
 * SMOVE - String move
 *
 * Format: t SMOVE <source/r/t/I1=>, <dest/w/t/I2=>
 *
 * Assembly:
 *   BI SMOVE (string move bits)        Hex 0xFD70
 *   BY SMOVE (string move bytes)       Hex 0xFD71
 *   H  SMOVE (string move halfwords)   Hex 0xFD72
 *   W  SMOVE (string move words)       Hex 0xFD73
 *   F  SMOVE (string move floats)      Hex 0xFD74
 *   D  SMOVE (string move doubles)     Hex 0xFD75
 *
 * Operation:
 *   while not end of strings do
 *     S(I1) -> D(I2)
 *     I1 + 1 -> I1, I2 + 1 -> I2
 *   enddo
 *
 * Description:
 *   Elements are moved from the <source> to the <dest> operand until
 *   the <source> operand is empty or the <dest> operand is full.
 *
 * Terminating conditions:
 *   - outside source: K=0 Z=0 I1,I2 unmodified, DR trap condition
 *   - outside dest: K=1 Z=0 I1,I2 unmodified, DR trap condition
 *   - source empty: K=0 Z=1 I1,I2 := next element
 *   - dest full: K=1 Z=0 I1,I2 := next element
 *
 * Reference: ND-500 Reference Manual, Chapter 14.2
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Smove.cs
 */
void nd500_instr_Smove(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] SMOVE at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t dest_desc_addr = fi->operands[1].effective_address;

    /* Load string descriptors */
    Nd500StringDescriptor source_desc, dest_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, false, false, &source_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, false, false, &dest_desc)) {
        return;
    }

    /* Get starting indices from I1 and I2 */
    uint32_t src_index = cpu->I[0];   /* I1 */
    uint32_t dest_index = cpu->I[1];  /* I2 */

    /* Move elements using proper data type handling */
    while (src_index < source_desc.element_count && dest_index < dest_desc.element_count) {
        /* Read from source */
        uint64_t value = nd500_string_read_element(cpu, &source_desc, src_index, fi->data_type);

        /* Write to destination */
        nd500_string_write_element(cpu, &dest_desc, dest_index, value, fi->data_type);

        src_index++;
        dest_index++;
    }

    /* Update index registers */
    cpu->I[0] = src_index;
    cpu->I[1] = dest_index;

    /* Set status flags */
    /* Z is left 0 on completion. The real B30 microcode terminator is ALU,A A,BM00 B,X1 ST,SAVA
       (A,BM00 = 1<<0 = 1) -> Z=0; the termination REASON is carried in K (SOUR_RANGE @003117 K,ZRO ->
       source exhausted K=0, DEST_RANGE @003122 K,ONE -> dest full K=1), never re-latching Z. The green
       SFILL follows this; setting Z=1 diverged from the microword on every completed move. */
    nd500_clear_flag(cpu, ND500_FLAG_Z);
    if (src_index >= source_desc.element_count) {
        /* Source exhausted -> K=0 */
        nd500_clear_flag(cpu, ND500_FLAG_K);
    } else if (dest_index >= dest_desc.element_count) {
        /* Destination full -> K=1 */
        nd500_set_flag(cpu, ND500_FLAG_K);
    }

    nd500_string_clear_unused_flags(cpu);  /* Clears S, C, O */
}
