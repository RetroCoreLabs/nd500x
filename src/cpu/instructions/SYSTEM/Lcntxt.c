/*
 * Lcntxt.c - ND-500 Lcntxt instruction (SYSTEM class)
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
 * Lcntxt instruction - SYSTEM class
 *
 * LCNTXT - Load Context Block
 *
 * Format: LCNTXT <mask/r/W>, <address/r/W>, <process number/r/W>
 *
 * Assembly:
 *   LCNTXT (load context)                    Hex 0xFFF8
 *
 * Operation: Load context block registers from specified address according to mask.
 *
 * Description:
 *   Privileged instruction that loads context block registers from the specified
 *   address according to the mask. The mask determines which registers are loaded.
 *   This instruction is used for process context switching and system operations.
 *   The process number parameter is used for addressing the context save area:
 *   context_address = (process_number+1)*400B + address
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
 * Reference: ND-500 Reference Manual, Chapter 16.27.4
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Lcntxt.cs
 */
void nd500_instr_Lcntxt(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 41-45) */
    if (fi->operand_count != 3) {
        printf("[ERROR] LCNTXT at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - LCNTXT is a privileged instruction (like C# lines 53-57) */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* Read operands (like C# lines 48-50) */
    uint32_t mask = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    uint32_t address = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);
    uint32_t process_number = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[2], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* A nonzero address is the context block itself (manual 16.27.4; B30
     * LOADCT_13 @010777 goes straight to LOADCT_14 when the address is not
     * zero). Only address 0 means the process's own save area, (process
     * number + 1) * 400B plus an operating-system base that nd500x takes as 0
     * (the microcode's GET_CNTXT reads it from a register not modelled here).
     * This always added (process number + 1) * 256 before; NDIX passes
     * process -1 (lcntxt CNTXMASK,r4,$-1, locore.c), so it saw no difference. */
    uint32_t context_address = address != 0 ? address : (process_number + 1) * 256u;

    /* Each selected word sits at its FIXED slot word*4 in the context block,
     * not packed by mask: the kernel reads the same block through fixed CX_
     * offsets (CX_ST1 = 64, CX_CED = 92; machine/locore.h). The address is
     * PHYSICAL (kernel phyladr) - bypass the MMU, matching Scntxt. */
    for (unsigned bit = 0; bit < ND500_REGBLOCK_MASK_BITS; bit++) {
        int words[2];
        int n = (mask >> bit) & 1u ? nd500_regblock_words(bit, words) : 0;
        for (int k = 0; k < n; k++) {
            uint32_t* reg = nd500_regblock_register(cpu, words[k]);
            if (reg) {
                *reg = nd500_bus_read32(cpu->machine, context_address + (uint32_t)words[k] * 4u);
            }
        }
    }

    /* No status bits affected for this instruction */
}
