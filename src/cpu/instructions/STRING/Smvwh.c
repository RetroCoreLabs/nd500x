/*
 * Smvwh.c - ND-500 SMVWH instruction (STRING class)
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
 * SMVWH instruction - STRING class
 *
 * SMVWH - String move while
 *
 * Format: BY SMVWH <source/r/BY/I1=>, <dest/w/BY/I2=>, <mask/r/BY>, <test/r/BY>
 *
 * Assembly:
 *   BY SMVWH (byte string move while)  Hex 0xFD72
 *
 * Operation:
 *   while not end of strings and S(I1) AND <mask> = <test> do
 *     S(I1) -> D(I2), I1 + 1 -> I1, I2 + 1 -> I2
 *   enddo
 *
 * Description:
 *   Bytes are moved from <source> to <dest> while (element AND mask) == test.
 *   Moving continues until source is empty, dest is full, or condition fails.
 *
 * Terminating conditions:
 *   - different bytes: K=0 Z=0
 *   - source empty: K=0 Z=1
 *   - dest full: K=1 Z=1
 *
 * Reference: ND-500 Reference Manual, Chapter 14.3
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Smvwh.cs
 */
void nd500_instr_Smvwh(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 4) {
        printf("[ERROR] SMVWH at PC=0x%08X: Expected 4 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses and mask/test values */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t dest_desc_addr = fi->operands[1].effective_address;
    uint32_t mask = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[2], fi->data_type);
    uint32_t test = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[3], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Load string descriptors */
    Nd500StringDescriptor source_desc, dest_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, &source_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, &dest_desc)) {
        return;
    }

    /* Get starting indices */
    uint32_t src_index = cpu->I[0];
    uint32_t dest_index = cpu->I[1];

    /* Copy while condition is met, until source empty or dest full */
    bool different_bytes = false;
    while (src_index < source_desc.element_count && dest_index < dest_desc.element_count) {
        /* Read source element */
        uint32_t src_addr = source_desc.base_address + src_index;
        uint8_t element = nd500_read_memory_8(cpu, src_addr);

        /* Check while condition: (element AND mask) == test */
        if ((element & mask) != test) {
            different_bytes = true;
            break;
        }

        /* Move element to destination */
        uint32_t dest_addr = dest_desc.base_address + dest_index;
        nd500_write_memory_8(cpu, dest_addr, element);
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
    if (different_bytes) {
        /* Condition no longer met */
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    } else if (src_index >= source_desc.element_count) {
        /* Source empty -> K=0 Z=1. B30 SMVWHBY_F04 @007170 is ALU,FZRO ST,SAVA
         * (Z=1) before the byte loop, then @007171 K,ZRO; only the different-
         * byte exit @007204 (A,BM02 ST,SAVA) overwrites Z with 0. The old
         * comment read the A,BM00 ST,SAVA words, which are the range-trap
         * exits (SOUR_RANGE/DEST_RANGE). The manual's table, one row out of
         * line in this copy, says the same. */
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else if (dest_index >= dest_desc.element_count) {
        /* Dest full -> K=1 Z=1 (@007172 K,ONE after the same FZRO). */
        nd500_set_flag(cpu, ND500_FLAG_K);
        nd500_set_flag(cpu, ND500_FLAG_Z);
    }

    nd500_clear_flag(cpu, ND500_FLAG_S);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
