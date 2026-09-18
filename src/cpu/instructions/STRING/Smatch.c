/*
 * Smatch.c - ND-500 SMATCH instruction (STRING class)
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
 * SMATCH instruction - STRING class
 *
 * SMATCH - String Match (Manual 14.18)
 *
 * Format: BY SMATCH <substring/r/BY/I1=>, <string/r/BY/I2=>
 * Opcode: 0xFDB3 / 176663B
 * Microcode: 001344 SMATCH -> 007244 SMATCHBY_F01 (outer 007257, inner 007265-007276).
 *
 * Operation:
 *   subptr = I1 (kept; I1 is NOT modified)
 *   while not end-of-string:
 *       if substring == string[I2 .. I2+sublen-1] byte-for-byte:
 *           Z=1 ; I2 :- first matching byte ; stop
 *       I2 += 1
 *   end
 *
 * Description:
 *   Naive substring search. <substring> (operand 0, indexed by I1, which is left
 *   unmodified) is searched for within <string> (operand 1, indexed by I2). On a
 *   match, I2 points at the first matching byte. This is a two-operand match, NOT
 *   a three-operand copy; the destination index that advances is I2, not I1.
 *
 * Terminating conditions (manual 14.18):
 *   - substring outside          : K=0 Z=1 ; DR trap ; I2 unchanged
 *   - string outside (sub inside): K=0 Z=0 ; DR trap ; I2 unchanged
 *   - substring found            : K=0 Z=1 ; I2 :- first matching byte
 *   - string exhausted (no match): K=0 Z=0 ; I2 :- next element
 *
 * Data Status Bits: K CLEARED, Z CONDITIONAL, C/O/S CLEARED.
 *
 * Traps: DR trap (an operand addressed outside its string).
 *
 * Reference: ND-500 Reference Manual, Section 14.18.
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Smatch.cs
 */
void nd500_instr_Smatch(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] SMATCH at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Operand 0: substring descriptor (indexed by I1, kept).
     * Operand 1: string descriptor (indexed by I2, advances). */
    uint32_t substr_desc_addr = fi->operands[0].effective_address;
    uint32_t string_desc_addr = fi->operands[1].effective_address;

    Nd500StringDescriptor substr_desc, string_desc;
    if (!nd500_load_string_descriptor(cpu, substr_desc_addr, false, false, &substr_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, string_desc_addr, false, false, &string_desc)) {
        return;
    }

    /* subptr = I1 (kept). The substring runs from I1 to the end of its descriptor. */
    uint32_t sub_start = cpu->I[0];
    uint32_t sublen = (substr_desc.element_count > sub_start)
                      ? (substr_desc.element_count - sub_start) : 0;

    uint32_t str_index = cpu->I[1];

    /* Naive substring search: advance I2 until the substring matches at I2. */
    bool found = false;
    while (str_index + sublen <= string_desc.element_count) {
        bool match = true;
        for (uint32_t i = 0; i < sublen; i++) {
            uint8_t s = nd500_read_memory_8(cpu, substr_desc.base_address + sub_start + i);
            uint8_t d = nd500_read_memory_8(cpu, string_desc.base_address + str_index + i);
            if (s != d) {
                match = false;
                break;
            }
        }
        if (match) {
            found = true;
            break;
        }
        str_index++;
    }

    /* I1 is left unmodified; I2 updated to the match position or next element. */
    if (!found) {
        str_index = string_desc.element_count;
    }
    cpu->I[1] = str_index;

    /* Status: K/S/C/O cleared; Z=1 if found, Z=0 if the string was exhausted. */
    if (found) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }
    nd500_clear_flag(cpu, ND500_FLAG_K);
    nd500_clear_flag(cpu, ND500_FLAG_S);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
