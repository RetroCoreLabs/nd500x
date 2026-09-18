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
 *   Register numbering (bits in mask):
 *   1=P, 2=L, 3=B, 4=R, 5-8=I1-I4, 9-12=A1-A4, 13-16=E1-E4,
 *   17-18=ST1-ST2, 19=PS, 20=TOS, 21=LL, 22=HL, 23=THA, 24-25=CED-CAD,
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

    /* Save registers based on mask bits (like C# lines 68-125) */
    for (int reg_num = 1; reg_num <= 37; reg_num++) {
        if ((mask & (1u << (reg_num - 1))) != 0) {
            uint32_t reg_value = 0;

            /* Map register number to actual register */
            switch (reg_num) {
                case 1:  reg_value = cpu->PC; break;        /* P register */
                case 2:  reg_value = cpu->L; break;         /* L register */
                case 3:  reg_value = cpu->B; break;         /* B register */
                case 4:  reg_value = cpu->R; break;         /* R register */
                case 5:  reg_value = cpu->I[0]; break;      /* I1 register */
                case 6:  reg_value = cpu->I[1]; break;      /* I2 register */
                case 7:  reg_value = cpu->I[2]; break;      /* I3 register */
                case 8:  reg_value = cpu->I[3]; break;      /* I4 register */
                case 9:  reg_value = cpu->A[0]; break;      /* A1 register */
                case 10: reg_value = cpu->A[1]; break;      /* A2 register */
                case 11: reg_value = cpu->A[2]; break;      /* A3 register */
                case 12: reg_value = cpu->A[3]; break;      /* A4 register */
                case 13: reg_value = cpu->E[0]; break;      /* E1 register */
                case 14: reg_value = cpu->E[1]; break;      /* E2 register */
                case 15: reg_value = cpu->E[2]; break;      /* E3 register */
                case 16: reg_value = cpu->E[3]; break;      /* E4 register */
                case 17: reg_value = cpu->ST1; break;       /* ST1 register */
                case 18: reg_value = cpu->ST2; break;       /* ST2 register */
                case 19: reg_value = cpu->PS; break;        /* PS register */
                case 20: reg_value = cpu->TOS; break;       /* TOS register */
                case 21: reg_value = cpu->LL; break;        /* LL register */
                case 22: reg_value = cpu->HL; break;        /* HL register */
                case 23: reg_value = cpu->THA; break;       /* THA register */
                case 24: reg_value = cpu->CED; break;       /* CED register */
                case 25: reg_value = cpu->CAD; break;       /* CAD register */
                /* 26-29: mic1-mic4 (not in C structure) */
                case 30: reg_value = cpu->OTE1; break;      /* OTE1 register */
                case 31: reg_value = cpu->OTE2; break;      /* OTE2 register */
                case 32: reg_value = cpu->CTE1; break;      /* CTE1 register */
                case 33: reg_value = cpu->CTE2; break;      /* CTE2 register */
                case 34: reg_value = cpu->MTE1; break;      /* MTE1 register */
                case 35: reg_value = cpu->MTE2; break;      /* MTE2 register */
                case 36: reg_value = cpu->TEMM1; break;     /* TEMM1 register */
                case 37: reg_value = cpu->TEMM2; break;     /* TEMM2 register */
                default: continue;
            }

            /* Write register value to context memory. The context-block address
             * is PHYSICAL: the kernel passes phyladr(_cxbtab)+ipl*256 (splx4,
             * intvec, locore.c), so this must bypass the data MMU. Using the
             * MMU path (nd500_write_memory_32) mis-writes the block and the
             * saved CX_B reads back 0 -> intvec "kernel stack underflow" panic.
             * Matches RetroCore CpuND500.ProcessControl ReadPhysical32/WritePhysical32.
             * Register N is at a FIXED slot (reg_num-1)*4, matching the kernel's
             * fixed CX_ offsets and lregbl/Lcntxt (NOT consecutive mask packing). */
            uint32_t current_address = context_address + (uint32_t)(reg_num - 1) * 4u;
            nd500_bus_write32(cpu->machine, current_address, reg_value);
        }
    }

    /* No status bits affected for this instruction */
}
