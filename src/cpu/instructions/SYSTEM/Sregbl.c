/*
 * Sregbl.c - ND-500 Sregbl instruction (SYSTEM class)
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
 *   locations addressed by <address> plus register number*4. When executed
 *   in non-privileged mode, the 'mask' will be reduced to include only
 *   registers that may be accessed by assembly instructions in non-privileged
 *   mode. Registers residing in the domain information table are read and
 *   stored in the save area if included in the mask.
 *
 *   Register numbering (bits in mask):
 *   1=P, 2=L, 3=B, 4=R, 5-8=I1-I4, 9-12=A1-A4, 13-16=E1-E4,
 *   17-18=ST1-ST2, 19=PS, 20=TOS, 21=LL, 22=HL, 23=THA, 24-25=CED-CAD
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

    /* Apply privilege restrictions for non-privileged mode (like C# lines 55-63) */
    /* Non-privileged mode cannot access: ST2 (18), PS (19), CED (24), CAD (25), CTE (32-33), MTE (34-35), TEMM (36-37) */
    /* Note: Only lower 32 bits of mask are used - bits 32-37 need 64-bit mask */
    if (!(cpu->ST1 & ND500_FLAG_PIA)) {
        uint32_t privileged_mask = (1u << 17) | (1u << 18) | (1u << 23) | (1u << 24);  /* ST2, PS, CED, CAD */
        mask &= ~privileged_mask;
        /* Bits 32-37 cannot be set in a 32-bit mask anyway, so they're implicitly excluded */
    }

    /* Save registers based on mask bits (like C# lines 68-124) */
    /* Address calculation: <address> + register_number*4 */
    for (int reg_num = 1; reg_num <= 37; reg_num++) {
        if ((mask & (1u << (reg_num - 1))) != 0) {
            /* Register N is saved at address + (N-1)*4 (reg 1 = P at offset 0),
             * matching lregbl and the manual register-block layout (Fig.2). */
            uint32_t reg_address = address + (uint32_t)((reg_num - 1) * 4);
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
                case 18: reg_value = cpu->ST2; break;       /* ST2 register (privileged) */
                case 19: reg_value = cpu->PS; break;        /* PS register (privileged) */
                case 20: reg_value = cpu->TOS; break;       /* TOS register */
                case 21: reg_value = cpu->LL; break;        /* LL register */
                case 22: reg_value = cpu->HL; break;        /* HL register */
                case 23: reg_value = cpu->THA; break;       /* THA register */
                case 24: reg_value = cpu->CED; break;       /* CED register (privileged) */
                case 25: reg_value = cpu->CAD; break;       /* CAD register (privileged) */
                /* 26-29: mic1-mic4 (not implemented) */
                case 30: reg_value = cpu->OTE1; break;      /* OTE1 register */
                case 31: reg_value = cpu->OTE2; break;      /* OTE2 register */
                case 32: reg_value = cpu->CTE1; break;      /* CTE1 register (privileged) */
                case 33: reg_value = cpu->CTE2; break;      /* CTE2 register (privileged) */
                case 34: reg_value = cpu->MTE1; break;      /* MTE1 register (privileged) */
                case 35: reg_value = cpu->MTE2; break;      /* MTE2 register (privileged) */
                case 36: reg_value = cpu->TEMM1; break;     /* TEMM1 register (privileged) */
                case 37: reg_value = cpu->TEMM2; break;     /* TEMM2 register (privileged) */
                default: continue;
            }

            /* Write register value to logical memory */
            nd500_write_memory_32(cpu, reg_address, reg_value);
        }
    }

    /* No status bits affected for this instruction */
}
