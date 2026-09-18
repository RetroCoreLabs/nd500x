/*
 * Scopa.c - ND-500 SCOPA instruction (STRING class)
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
 * SCOPA instruction - STRING class
 *
 * Mnemonic: scopa    Opcode: 0xFDBE (176676 octal)
 * Format:  BY SCOPA <source-1/r/BY/I1=>, <source-2/r/BY/I2=>, <pad/r/BY>
 *
 * SCOPA - "String COmpare with PAd" (NOT a copy, and NO translation table).
 *
 * Compare byte string <source-1> (indexed by I1) with <source-2> (indexed by I2).
 * If the strings are of unequal length, the SHORTER string is logically extended
 * with <pad> bytes so both are the same length, and the comparison continues.
 * The THIRD operand is the PAD BYTE VALUE - a scalar, NOT a descriptor address.
 *
 * (The previous implementation modelled SCOPA as "string copy all" with a
 *  translation-table descriptor loaded from the 3rd operand's address. That is a
 *  different, non-existent instruction: for a constant pad operand like `$0` it
 *  dereferenced address 0 and took a protection violation. The ND LINKER's
 *  startup `BY SCOPA b.x, b.y, $0` hit exactly that. Corrected against the
 *  ND-500 Reference Manual ND-05.009.4 EN sect 14.12 p256, opcode 176676B.)
 *
 * Operation (manual):
 *   while not end of both strings and S(I1) = D(I2) do  I1+1->I1, I2+1->I2  enddo
 *   The shorter string is padded with <pad>; an index is only advanced while it
 *   is still inside its own string (padding does not advance that index).
 *
 * Terminating conditions (ND-500 Ref Manual p256):
 *   | condition                  | K | Z | S | I1,I2                 |
 *   | exact match (incl. pad)    | 0 | 1 | 0 | :- next element       |
 *   | greater byte in source-1   | 1 | 0 | 0 | :- differing elements |
 *   | smaller byte in source-1   | 1 | 0 | 1 | :- differing elements |
 *   (S convention matches SCOMP/COMP: source1 < source2 -> S=1.)
 *   C and O are cleared for all string operations (p244).
 */
void nd500_instr_Scopa(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 3) {
        printf("[ERROR] SCOPA at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* operand[0]/[1] are string descriptors; operand[2] is the pad BYTE VALUE. */
    Nd500StringDescriptor desc1, desc2;
    if (!nd500_load_string_descriptor(cpu, fi->operands[0].effective_address, &desc1)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, fi->operands[1].effective_address, &desc2)) {
        return;
    }
    uint8_t pad = (uint8_t)nd500_read_operand_value(cpu, &fi->operands[2], ND500_DTYPE_BYTE);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    uint32_t index1 = cpu->I[0];  /* I1 */
    uint32_t index2 = cpu->I[1];  /* I2 */

    bool strings_equal = true;
    bool string1_less = false;

    /* Compare until BOTH strings are exhausted; an exhausted string yields pad. */
    while (index1 < desc1.element_count || index2 < desc2.element_count) {
        uint8_t element1 = (index1 < desc1.element_count)
            ? nd500_read_memory_8(cpu, desc1.base_address + index1) : pad;
        uint8_t element2 = (index2 < desc2.element_count)
            ? nd500_read_memory_8(cpu, desc2.base_address + index2) : pad;

        if (element1 != element2) {
            strings_equal = false;
            string1_less = (element1 < element2);
            break;
        }

        /* Advance each index only while it is still inside its own string
         * (padding does not advance that string's index). */
        if (index1 < desc1.element_count) index1++;
        if (index2 < desc2.element_count) index2++;
    }

    /* Update index registers with final positions */
    cpu->I[0] = index1;  /* I1 */
    cpu->I[1] = index2;  /* I2 */

    /* Flags. With padding there is no length-mismatch case: a byte-vs-pad
     * difference sets K=1 just like a real byte difference. */
    if (strings_equal) {
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_set_flag(cpu, ND500_FLAG_Z);
        nd500_clear_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_set_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
        if (string1_less) {
            nd500_set_flag(cpu, ND500_FLAG_S);   /* source1 < source2 -> S=1 */
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_S); /* source1 > source2 -> S=0 */
        }
    }

    /* C and O are always cleared for string operations (manual p244). */
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
