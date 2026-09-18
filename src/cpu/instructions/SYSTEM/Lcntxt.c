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
 *   Register numbering (bits in mask):
 *   1=P, 2=L, 3=B, 4=R, 5-8=I1-I4, 9-12=A1-A4, 13-16=E1-E4,
 *   17-18=ST1-ST2, 19=PS, 20=TOS, 21=LL, 22=HL, 23=THA, 24-25=CED-CAD,
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

    /* Calculate context save area address: (process_number+1)*400B + OS_address */
    /* Octal 400B = 256 decimal (like C# line 61) */
    uint32_t context_address = (process_number + 1) * 256 + address;

    /* Load registers based on mask bits (like C# lines 67-120) */
    for (int reg_num = 1; reg_num <= 37; reg_num++) {
        if ((mask & (1u << (reg_num - 1))) != 0) {
            /* Register N lives at a FIXED slot (reg_num-1)*4 in the context
             * block, NOT packed consecutively by mask. The kernel accesses the
             * SAME block via fixed CX_ offsets (CX_ST1=64=(17-1)*4, CX_CED=92=
             * (24-1)*4; machine/locore.h), and _intvec reads CX_CED at entry
             * then lcntxt-restores at exit on the same block - so a sparse mask
             * (e.g. CNTXMASK=0x1c3ffff: regs 1-18 + 23-25) must land THA/CED/CAD
             * at 88/92/96, not the consecutive 72/76/80. Same fix as lregbl. */
            uint32_t current_address = context_address + (uint32_t)(reg_num - 1) * 4u;
            /* Context-block address is PHYSICAL (kernel phyladr) - bypass MMU,
             * matching Scntxt and RetroCore ProcessControl ReadPhysical32. */
            uint32_t reg_value = nd500_bus_read32(cpu->machine, current_address);

            /* Map register number to actual register */
            switch (reg_num) {
                case 1:  cpu->PC = reg_value; break;        /* P register */
                case 2:  cpu->L = reg_value; break;         /* L register */
                case 3:  cpu->B = reg_value; break;         /* B register */
                case 4:  cpu->R = reg_value; break;         /* R register */
                case 5:  cpu->I[0] = reg_value; break;      /* I1 register */
                case 6:  cpu->I[1] = reg_value; break;      /* I2 register */
                case 7:  cpu->I[2] = reg_value; break;      /* I3 register */
                case 8:  cpu->I[3] = reg_value; break;      /* I4 register */
                case 9:  cpu->A[0] = reg_value; break;      /* A1 register */
                case 10: cpu->A[1] = reg_value; break;      /* A2 register */
                case 11: cpu->A[2] = reg_value; break;      /* A3 register */
                case 12: cpu->A[3] = reg_value; break;      /* A4 register */
                case 13: cpu->E[0] = reg_value; break;      /* E1 register */
                case 14: cpu->E[1] = reg_value; break;      /* E2 register */
                case 15: cpu->E[2] = reg_value; break;      /* E3 register */
                case 16: cpu->E[3] = reg_value; break;      /* E4 register */
                case 17: cpu->ST1 = reg_value; break;       /* ST1 register */
                case 18: cpu->ST2 = reg_value; break;       /* ST2 register */
                case 19: cpu->PS = reg_value; break;        /* PS register */
                case 20: cpu->TOS = reg_value; break;       /* TOS register */
                case 21: cpu->LL = reg_value; break;        /* LL register */
                case 22: cpu->HL = reg_value; break;        /* HL register */
                case 23: cpu->THA = reg_value; break;       /* THA register */
                case 24: cpu->CED = reg_value; break;       /* CED register */
                case 25: cpu->CAD = reg_value; break;       /* CAD register */
                /* 26-29: mic1-mic4 (not in C structure) */
                case 30: cpu->OTE1 = reg_value; break;      /* OTE1 register */
                case 31: cpu->OTE2 = reg_value; break;      /* OTE2 register */
                case 32: cpu->CTE1 = reg_value; break;      /* CTE1 register */
                case 33: cpu->CTE2 = reg_value; break;      /* CTE2 register */
                case 34: cpu->MTE1 = reg_value; break;      /* MTE1 register */
                case 35: cpu->MTE2 = reg_value; break;      /* MTE2 register */
                case 36: cpu->TEMM1 = reg_value; break;     /* TEMM1 register */
                case 37: cpu->TEMM2 = reg_value; break;     /* TEMM2 register */
                default: break;
            }
        }
    }

    /* No status bits affected for this instruction */
}
