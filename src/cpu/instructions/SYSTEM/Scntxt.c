/*
 * Scntxt.c - ND-500 Scntxt instruction (SYSTEM class)
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
 * Scntxt instruction - SYSTEM class
 *
 * SCNTXT - Save Context Block
 *
 * Format: SCNTXT <mask/r/W>, <address/r/W>
 *
 * Assembly:
 *   SCNTXT (save context)                    Hex 0xFFF9
 *
 * Operation: Store context block registers in specified address according to 'mask'.
 *
 * Description:
 *   Privileged instruction that stores context block registers to the specified
 *   address according to the mask. The mask determines which registers are saved.
 *   This instruction is used for process context switching and system operations.
 *   When context save area is used, this is addressed by:
 *   (process number+1)*400B + an operating system defined address.
 *
 *   Register block layout (mask bit -> words at offset word*4): see
 *   nd500_regblock_words in instruction_helpers.h. Bits 0-15 are P L B R
 *   I1-I4 A1-A4 E1-E4, bit 16 STS (ST1, ST2), bits 17-23 PS TOS LL HL THA CED
 *   CAD, bit 24 MIC, bits 25-28 OTE CTE MTE TEMM. An earlier version gave ST2
 *   its own mask bit 17, so PS..CAD each answered one bit too high; the
 *   manual's table is in octal and the B30 store loop (STORERG_1 @011630)
 *   tests PS with BM21 and CED with BM26, i.e. bits 17 and 22.
 *   30-31=OTE1-OTE2, 32-33=CTE1-CTE2, 34-35=MTE1-MTE2, 36-37=TEMM1-TEMM2
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.27.3
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Scntxt.cs
 */
void nd500_instr_Scntxt(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 42-46) */
    if (fi->operand_count != 2) {
        printf("[ERROR] SCNTXT at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - SCNTXT is a privileged instruction (like C# lines 53-57) */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* Read operands (like C# lines 49-50) */
    uint32_t mask = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    uint32_t address = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* For SCNTXT, address is the base address directly (like C# line 62) */
    uint32_t context_address = address;

    /* The context-block address is PHYSICAL: the kernel passes
     * phyladr(_cxbtab)+ipl*256 (splx4, intvec, locore.c), so this must bypass
     * the data MMU. Using the MMU path (nd500_write_memory_32) mis-writes the
     * block and the saved CX_B reads back 0 -> intvec "kernel stack underflow"
     * panic. Matches RetroCore CpuND500.ProcessControl WritePhysical32. */
    for (unsigned bit = 0; bit < ND500_REGBLOCK_MASK_BITS; bit++) {
        int words[2];
        int n = (mask >> bit) & 1u ? nd500_regblock_words(bit, words) : 0;
        for (int k = 0; k < n; k++) {
            const uint32_t* reg = nd500_regblock_register(cpu, words[k]);
            if (reg) {
                nd500_bus_write32(cpu->machine, context_address + (uint32_t)words[k] * 4u, *reg);
            }
        }
    }

    /* No status bits affected for this instruction */
}
