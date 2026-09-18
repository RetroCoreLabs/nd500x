/*
 * Sfilln.c - ND-500 SFILLN instruction (STRING class)
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

/**
 * SFILLN instruction - STRING class
 *
 * SFILLN - String fill n elements
 *
 * Format: tn SFILLN <dest/w/t/I2=>, <count/r/W>
 *
 * Assembly:
 *   BIn SFILLN (bit string fill n)           Hex 0xFD94+(n-1)
 *   BYn SFILLN (byte string fill n)          Hex 0xFD98+(n-1)
 *
 * Operation:
 *   0 -> i
 *   while not end of string and i < m do
 *     tn -> D(I2)
 *     I2 + 1 -> I2
 *     i + 1 -> i
 *   enddo
 *
 * Description:
 *   The first m elements from I2, or all elements from I2 to the end of the
 *   string if fewer, are filled with the data-type part of the register. The
 *   elements are of the instruction's data type: I2 and the descriptor length
 *   count elements, not bytes (manual 7.2.8).
 *
 * Terminating conditions (manual 14.9):
 *   - outside dest:      K=1 Z=0, I2 unmodified, DR trap condition
 *   - m elements filled: K=0 Z=1, I2 := next element
 *   - dest full:         K=1 Z=0, I2 := next element
 *
 * Reference: ND-500 Reference Manual, Chapter 14.9
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Sfilln.cs
 */
void nd500_instr_Sfilln(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] SFILLN at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor address and count */
    uint32_t dest_desc_addr = fi->operands[0].effective_address;
    uint32_t count = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Load string descriptor */
    Nd500StringDescriptor dest_desc;
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, &dest_desc)) {
        return;
    }

    uint64_t fill_value = nd500_read_register_by_type(cpu, fi->target_register, fi->data_type);
    uint32_t dest_index = cpu->I[1];

    /* S, C and O are data status bits the list does not name: reset (6.5.1). */
    nd500_string_clear_unused_flags(cpu);

    if (dest_index >= dest_desc.element_count) {
        nd500_set_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
        trap_descriptor_range(cpu, fi->address);
        return;
    }

    uint32_t i = 0;
    while (dest_index < dest_desc.element_count && i < count) {
        nd500_string_write_element(cpu, &dest_desc, dest_index, fill_value, fi->data_type);
        dest_index++;
        i++;
    }
    cpu->I[1] = dest_index;

    /* m filled: K=0 Z=1 (B30 SFILNBY @007101 ALU,FZRO ST,SAVA then @007102
     * K,ZRO); dest full: K=1 Z=0 (@007074 A,BM00 ST,SAVA -> DEST_RANGE).
     * Which of the two wins when m runs out exactly at the end of the string
     * is not verified; this takes the manual's order, m filled first. */
    if (i == count) {
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_set_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }
}
