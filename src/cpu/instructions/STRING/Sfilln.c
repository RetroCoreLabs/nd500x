/*
 * Sfilln.c - ND-500 SFILLN instruction (STRING class)
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
 *   for i = 1 to <count> do
 *     tn -> D(I2)
 *     I2 + 1 -> I2
 *   enddo
 *
 * Description:
 *   Exactly <count> elements are filled with the register value.
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
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, false, true, &dest_desc)) {
        return;
    }

    /* Get fill value from target register */
    uint32_t fill_value;
    if (fi->target_register >= 1 && fi->target_register <= 4) {
        fill_value = nd500_read_integer_register(cpu, fi->target_register);
    } else {
        fill_value = 0;
    }

    /* Get starting index from I2 */
    uint32_t dest_index = cpu->I[1];

    /* Fill exactly count elements */
    for (uint32_t i = 0; i < count; i++) {
        if (dest_index >= dest_desc.element_count) {
            break;
        }
        uint32_t dest_addr = dest_desc.base_address + dest_index;
        nd500_write_memory_8(cpu, dest_addr, (uint8_t)(fill_value & 0xFF));
        dest_index++;
    }

    /* Update I2 register */
    cpu->I[1] = dest_index;

    /* Set status flags */
    if (dest_index >= dest_desc.element_count) {
        nd500_set_flag(cpu, ND500_FLAG_K);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_K);
    }
    nd500_clear_flag(cpu, ND500_FLAG_Z);
    nd500_clear_flag(cpu, ND500_FLAG_S);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
