/*
 * Sspar.c - ND-500 SSPAR instruction (STRING class)
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
 * SSPAR instruction - STRING class
 *
 * Mnemonic: sspar   Opcode: 0xFDB4 (176664 octal)
 * Format:  BY SSPAR <string/rw/BY/I1>, <mode/r/BY>
 *
 * VALIDATED AGAINST THE MANUAL: ND-05.009.4 EN ND-500 Reference Manual,
 * section 14.19 "Set parity in string", opcode 176664B (0xFDB4). Verbatim:
 *     while not end of string do
 *         parity according to <mode> -> bit 7 of S(I1)
 *         I1 + 1 -> I1
 *     enddo
 *
 * SSPAR - "String Set PARity". Sets the parity bit (bit 7) of every byte in
 * <string> (from I1 to the end) according to <mode> (a SCALAR value, NOT a
 * descriptor):
 *   0 = clear parity (bit 7 := 0)
 *   1 = set parity   (bit 7 := 1)
 *   2 = even parity  (bit 7 makes the total number of 1-bits even)
 *   3 = odd parity   (bit 7 makes the total number of 1-bits odd)
 *   any other value -> illegal operand value trap.
 * Terminating condition: K = 1. (RetroCore C# Sspar.cs mirrors this.)
 *
 * (The previous implementation modelled SSPAR as "string span reverse" and
 *  loaded the 2nd operand as a SET descriptor, dereferencing the mode scalar as
 *  a descriptor address -> protection violation. The ND LINKER does
 *  `by sspar IND(b.x), 2`. Section reference corrected from 14.20 to the actual
 *  14.19; 14.20 is "Check parity in string", a different instruction.)
 */
void nd500_instr_Sspar(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] SSPAR at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* operand[0] = string descriptor (read-write); operand[1] = mode VALUE. */
    Nd500StringDescriptor str_desc;
    if (!nd500_load_string_descriptor(cpu, fi->operands[0].effective_address, false, true, &str_desc)) {
        return;
    }
    uint32_t mode = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_BYTE) & 0xFF;
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    if (mode > 3) {
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t index = cpu->I[0];  /* I1 */
    while (index < str_desc.element_count) {
        uint32_t addr = str_desc.base_address + index;
        uint8_t byte = nd500_read_memory_8(cpu, addr);
        uint8_t low7 = (uint8_t)(byte & 0x7F);

        /* number of 1-bits in the low 7 bits */
        unsigned ones = 0;
        for (uint8_t b = low7; b; b &= (uint8_t)(b - 1)) ones++;

        uint8_t bit7;
        switch (mode) {
            case 0:  bit7 = 0x00; break;                       /* clear */
            case 1:  bit7 = 0x80; break;                       /* set */
            case 2:  bit7 = (ones & 1u) ? 0x80 : 0x00; break;  /* even */
            default: bit7 = (ones & 1u) ? 0x00 : 0x80; break;  /* 3 = odd */
        }

        nd500_write_memory_8(cpu, addr, (uint8_t)(low7 | bit7));
        index++;
    }

    cpu->I[0] = index;  /* I1 ends at end of string */

    /* Terminating condition per manual: K = 1. C and O cleared for string ops. */
    nd500_set_flag(cpu, ND500_FLAG_K);
    nd500_clear_flag(cpu, ND500_FLAG_Z);
    nd500_clear_flag(cpu, ND500_FLAG_S);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
