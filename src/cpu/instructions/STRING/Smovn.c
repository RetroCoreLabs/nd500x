/*
 * Smovn.c - ND-500 SMOVN instruction (STRING class)
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
 * SMOVN instruction - STRING class
 *
 * SMOVN - String move n elements
 *
 * Format: t SMOVN <source/r/t/I1=>, <dest/w/t/I2=>, <count/r/W>
 *
 * Assembly:
 *   BI SMOVN (string move n bits)        Hex 0xFD76
 *   BY SMOVN (string move n bytes)       Hex 0xFD77
 *   H  SMOVN (string move n halfwords)   Hex 0xFD78
 *   W  SMOVN (string move n words)       Hex 0xFD79
 *   F  SMOVN (string move n floats)      Hex 0xFD7A
 *   D  SMOVN (string move n doubles)     Hex 0xFD7B
 *
 * Operation:
 *   for i = 1 to <count> do
 *     S(I1) -> D(I2)
 *     I1 + 1 -> I1, I2 + 1 -> I2
 *   enddo
 *
 * Description:
 *   Exactly <count> elements are moved from <source> to <dest>.
 *
 * Reference: ND-500 Reference Manual, Chapter 14.3
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Smovn.cs
 */
void nd500_instr_Smovn(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 3) {
        printf("[ERROR] SMOVN at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses and count */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t dest_desc_addr = fi->operands[1].effective_address;
    uint32_t count = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[2], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Load string descriptors */
    Nd500StringDescriptor source_desc, dest_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, false, false, &source_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, false, false, &dest_desc)) {
        return;
    }

    uint32_t src_index = cpu->I[0];
    uint32_t dest_index = cpu->I[1];

    /* outside source: K=0, outside dest: K=1; Z=0, I1 and I2 unmodified, DR
     * trap condition (manual 14.7). */
    if (src_index >= source_desc.element_count || dest_index >= dest_desc.element_count) {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
        if (src_index >= source_desc.element_count) {
            nd500_clear_flag(cpu, ND500_FLAG_K);
        } else {
            nd500_set_flag(cpu, ND500_FLAG_K);
        }
        nd500_string_clear_unused_flags(cpu);
        trap_descriptor_range(cpu, fi->address);
        return;
    }

    /* Elements are of the instruction's data type; the descriptor lengths and
     * I1/I2 count elements (manual 7.2.8). "Overlap is taken care of": when
     * the destination starts above the source, copy from the last element
     * down so no source element is overwritten before it is read. */
    uint32_t n = count;
    if (n > source_desc.element_count - src_index) n = source_desc.element_count - src_index;
    if (n > dest_desc.element_count - dest_index) n = dest_desc.element_count - dest_index;

    /* Compare start positions in bits, so BI strings (8 elements a byte) are
     * ordered the same way as the others. */
    uint64_t element_bits = (fi->data_type == ND500_DTYPE_BIT)
                                ? 1u : 8u * nd500_get_element_size(fi->data_type);
    uint64_t src_start = (uint64_t)source_desc.base_address * 8u + (uint64_t)src_index * element_bits;
    uint64_t dest_start = (uint64_t)dest_desc.base_address * 8u + (uint64_t)dest_index * element_bits;
    if (dest_start > src_start) {
        for (uint32_t k = n; k > 0; k--) {
            uint64_t v = nd500_string_read_element(cpu, &source_desc, src_index + k - 1, fi->data_type);
            nd500_string_write_element(cpu, &dest_desc, dest_index + k - 1, v, fi->data_type);
        }
    } else {
        for (uint32_t k = 0; k < n; k++) {
            uint64_t v = nd500_string_read_element(cpu, &source_desc, src_index + k, fi->data_type);
            nd500_string_write_element(cpu, &dest_desc, dest_index + k, v, fi->data_type);
        }
    }
    src_index += n;
    dest_index += n;

    cpu->I[0] = src_index;
    cpu->I[1] = dest_index;

    /* Terminating conditions (manual 14.7):
     *   m items moved: K=0 Z=1 - B30 SMOVNBY_F67 @007440 ALU,FZRO ST,SAVA,
     *                  reached from the count test @007436 COND,MCNZ;
     *   source empty:  K=0 Z=0;  dest full: K=1 Z=0 - the A,BM00 ST,SAVA words
     *                  @007427/@007431 (SOUR_RANGE/DEST_RANGE).
     * An earlier version left Z=0 on every completed move, reading only the
     * A,BM00 words; the microword engine run on the corpus gives Z=1 for m
     * moved. Which ending wins when m runs out exactly at the end of a string
     * is not verified; this takes the manual's order, m moved first. */
    nd500_string_clear_unused_flags(cpu);  /* S, C, O */
    if (n == count) {
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else if (dest_index >= dest_desc.element_count) {
        nd500_set_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }
}
