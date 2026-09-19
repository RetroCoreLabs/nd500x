/*
 * Sregbl.c - ND-500 Sregbl instruction (SYSTEM class)
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
 * Sregbl instruction - SYSTEM class
 *
 * SREGBL - Save Register Block
 *
 * Format: SREGBL <mask/r/W>, <address/r/W>
 *
 * Assembly:
 *   SREGBL (save register block)             Hex 0xFFF7
 *
 * Operation: Save register block to logical address according to 'mask'.
 *
 * Description:
 *   The registers specified in the mask are saved to logical memory
 *   locations addressed by <address> plus register number*4. Registers
 *   residing in the domain information table are read and stored in the save
 *   area if included in the mask. The mask is not reduced in non-privileged
 *   mode: the manual says that only of LREGBL, and the store loop has no
 *   privilege test (only LOADRG_1 tests MIC,STS).
 *
 *   Register block layout (mask bit -> words at offset word*4): see
 *   nd500_regblock_words in instruction_helpers.h. Bits 0-15 are P L B R
 *   I1-I4 A1-A4 E1-E4, bit 16 STS (ST1, ST2), bits 17-23 PS TOS LL HL THA CED
 *   CAD, bit 24 MIC, bits 25-28 OTE CTE MTE TEMM. An earlier version gave ST2
 *   its own mask bit 17, so PS..CAD each answered one bit too high; the
 *   manual's table is in octal and the B30 store loop (STORERG_1 @011630)
 *   tests PS with BM21 and CED with BM26, i.e. bits 17 and 22.
 *
 * Trap conditions: Addressing traps
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.27.1
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Sregbl.cs
 */
void nd500_instr_Sregbl(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 43-47) */
    if (fi->operand_count != 2) {
        printf("[ERROR] SREGBL at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands (like C# lines 50-51) */
    uint32_t mask = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    uint32_t address = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    for (unsigned bit = 0; bit < ND500_REGBLOCK_MASK_BITS; bit++) {
        int words[2];
        int n = (mask >> bit) & 1u ? nd500_regblock_words(bit, words) : 0;
        for (int k = 0; k < n; k++) {
            const uint32_t* reg = nd500_regblock_register(cpu, words[k]);
            if (reg) {                   /* MIC is not emulated: its words are left as they are */
                nd500_write_memory_32(cpu, address + (uint32_t)words[k] * 4u, *reg);
            }
        }
    }

    /* No status bits affected for this instruction */
}
