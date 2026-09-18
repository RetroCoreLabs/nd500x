/*
 * Sspan.c - ND-500 SSPAN instruction (STRING class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SSPAN instruction - STRING class
 *
 * SSPAN - String Span (Manual 14.17)
 *
 * Format: BY SSPAN <source/r/BY/I1=>, <mask/r/BY>, <trans table/aa/BY>
 * Opcode: 0xFDB2 / 176662B
 * Microcode: 001341 SSPAN -> DIS_IDESC -> 007222 SSPANBY_F01 (mask test 007241).
 *
 * Operation:
 *   while not end-of-source and (table[S(I1)] AND mask) != 0:
 *       I1 += 1
 *   end
 *
 * Description:
 *   The complement of SSCAN: span (skip) elements while the translated,
 *   masked element is nonzero; stop at the first element whose translated,
 *   masked value is zero, or at the end of the source.
 *
 * Terminating conditions (manual 14.17):
 *   - source outside:              K=0 Z=0 ; DR trap ; I1 unchanged
 *   - (tr(byte) AND mask) == 0:    K=0 Z=1 ; I1 :- found element
 *   - source empty / end reached:  K=0 Z=0 ; I1 :- next element
 *
 * Data Status Bits: K CLEARED, Z CONDITIONAL, C/O/S CLEARED.
 *
 * Traps: DR trap (source outside string); ILL_OP_SPEC (bad table, 007225).
 *
 * Reference: ND-500 Reference Manual, Section 14.17.
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Sspan.cs
 */
void nd500_instr_Sspan(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 3) {
        printf("[ERROR] SSPAN at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint8_t  mask = (uint8_t)nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    uint32_t table_addr = fi->operands[2].effective_address;

    Nd500StringDescriptor source_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, false, true, &source_desc)) {
        return;
    }

    uint32_t src_index = cpu->I[0];

    /* Span while (table[S(I1)] AND mask) != 0, stopping when it becomes zero */
    bool stopped_on_zero = false;
    while (src_index < source_desc.element_count) {
        uint8_t element = nd500_read_memory_8(cpu, source_desc.base_address + src_index);
        uint8_t translated = nd500_read_memory_8(cpu, table_addr + element);
        if ((translated & mask) == 0) {
            stopped_on_zero = true;
            break;
        }
        src_index++;
    }

    /* Update I1: stopping element, or next element (end) */
    cpu->I[0] = src_index;

    /* Status: K/S/C/O cleared; Z=1 if stopped on a masked-zero element,
     * Z=0 if the source was exhausted/empty (manual 14.17). */
    if (stopped_on_zero) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }
    nd500_clear_flag(cpu, ND500_FLAG_K);
    nd500_clear_flag(cpu, ND500_FLAG_S);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
