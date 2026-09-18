/*
 * Sscan.c - ND-500 SSCAN instruction (STRING class)
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
 * SSCAN instruction - STRING class
 *
 * SSCAN - String Scan (Manual 14.16)
 *
 * Format: BY SSCAN <source/r/BY/I1=>, <mask/r/BY>, <trans table/aa/BY>
 * Opcode: 0xFDB1 / 176661B
 * Microcode: 001336 SSCAN -> DIS_IDESC -> 007200 SSCANBY_F01 (mask test 007217-007220).
 *
 * Operation:
 *   while not end-of-source and (table[S(I1)] AND mask) == 0:
 *       I1 += 1
 *   end
 *
 * Description:
 *   Scan the <source> string, translating each element through the 256-byte
 *   <trans table> and ANDing with <mask>, until a translated element has any
 *   masked bit set (found) or the end of the source is reached. This is NOT a
 *   plain equal-byte scan - both the mask and the translate table participate.
 *
 * Terminating conditions (manual 14.16):
 *   - source outside:              K=0 Z=1 ; DR trap ; I1 unchanged
 *   - (tr(byte) AND mask) != 0:    K=0 Z=0 ; I1 :- found element
 *   - source empty / end reached:  K=0 Z=1 ; I1 :- next element
 *
 * Data Status Bits: K CLEARED, Z CONDITIONAL, C/O/S CLEARED.
 *
 * Traps: DR trap (source outside string); ILL_OP_SPEC (bad table, 007203).
 *
 * Reference: ND-500 Reference Manual, Section 14.16.
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Sscan.cs
 */
void nd500_instr_Sscan(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 3) {
        printf("[ERROR] SSCAN at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Operand 0: source string descriptor (absolute address).
     * Operand 1: mask byte.
     * Operand 2: translate table base (256-byte table, absolute address). */
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

    /* Scan until (table[S(I1)] AND mask) != 0, or end of source */
    bool found = false;
    while (src_index < source_desc.element_count) {
        uint8_t element = nd500_read_memory_8(cpu, source_desc.base_address + src_index);
        uint8_t translated = nd500_read_memory_8(cpu, table_addr + element);
        if ((translated & mask) != 0) {
            found = true;
            break;
        }
        src_index++;
    }

    /* Update I1: found element, or next element (end) */
    cpu->I[0] = src_index;

    /* Status: K/S/C/O cleared; Z=0 if found, Z=1 if end/empty reached */
    if (found) {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    }
    nd500_clear_flag(cpu, ND500_FLAG_K);
    nd500_clear_flag(cpu, ND500_FLAG_S);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
