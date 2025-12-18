#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Lregbl instruction - SYSTEM class
 *
 * LREGBL - Load Register Block
 *
 * Format: LREGBL <mask/r/W>, <address/r/W>
 *
 * Assembly:
 *   LREGBL (load register block)             Hex 0xFFF6
 *
 * Operation: Load register block from logical address according to 'mask'.
 *
 * Description:
 *   The registers specified in the mask are loaded from logical memory
 *   locations addressed by <address> plus register number*4. When executed
 *   in non-privileged mode, the 'mask' will be reduced to include only
 *   registers that may be modified by assembly instructions in non-privileged
 *   mode. Registers residing in the domain information table are handled
 *   according to the mask specification.
 *
 *   Register numbering (bits in mask):
 *   1=P, 2=L, 3=B, 4=R, 5-8=I1-I4, 9-12=A1-A4, 13-16=E1-E4,
 *   17-18=ST1-ST2, 19=PS, 20=TOS, 21=LL, 22=HL, 23=THA, 24-25=CED-CAD
 *
 * Trap conditions: Addressing traps
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.27.2
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Lregbl.cs
 */
void nd500_instr_Lregbl(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 43-47) */
    if (fi->operand_count != 2) {
        printf("[ERROR] LREGBL at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands (like C# lines 50-51) */
    uint32_t mask = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    uint32_t address = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);

    /* Apply privilege restrictions for non-privileged mode (like C# lines 55-63) */
    /* Non-privileged mode cannot modify: ST2 (18), PS (19), CED (24), CAD (25), CTE (32-33), MTE (34-35), TEMM (36-37) */
    /* Note: Only lower 32 bits of mask are used - bits 32-37 need 64-bit mask */
    if (!(cpu->ST1 & ND500_FLAG_PIA)) {
        uint32_t privileged_mask = (1u << 17) | (1u << 18) | (1u << 23) | (1u << 24);  /* ST2, PS, CED, CAD */
        mask &= ~privileged_mask;
        /* Bits 32-37 cannot be set in a 32-bit mask anyway, so they're implicitly excluded */
    }

    /* Load registers based on mask bits (like C# lines 68-122) */
    /* Address calculation: <address> + register_number*4 */
    for (int reg_num = 1; reg_num <= 37; reg_num++) {
        if ((mask & (1u << (reg_num - 1))) != 0) {
            uint32_t reg_address = address + (uint32_t)(reg_num * 4);
            uint32_t reg_value = nd500_read_memory_32(cpu, reg_address);

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
                case 18: cpu->ST2 = reg_value; break;       /* ST2 register (privileged) */
                case 19: cpu->PS = reg_value; break;        /* PS register (privileged) */
                case 20: cpu->TOS = reg_value; break;       /* TOS register */
                case 21: cpu->LL = reg_value; break;        /* LL register */
                case 22: cpu->HL = reg_value; break;        /* HL register */
                case 23: cpu->THA = reg_value; break;       /* THA register */
                case 24: cpu->CED = reg_value; break;       /* CED register (privileged) */
                case 25: cpu->CAD = reg_value; break;       /* CAD register (privileged) */
                /* 26-29: mic1-mic4 (not implemented) */
                case 30: cpu->OTE1 = reg_value; break;      /* OTE1 register */
                case 31: cpu->OTE2 = reg_value; break;      /* OTE2 register */
                case 32: cpu->CTE1 = reg_value; break;      /* CTE1 register (privileged) */
                case 33: cpu->CTE2 = reg_value; break;      /* CTE2 register (privileged) */
                case 34: cpu->MTE1 = reg_value; break;      /* MTE1 register (privileged) */
                case 35: cpu->MTE2 = reg_value; break;      /* MTE2 register (privileged) */
                case 36: cpu->TEMM1 = reg_value; break;     /* TEMM1 register (privileged) */
                case 37: cpu->TEMM2 = reg_value; break;     /* TEMM2 register (privileged) */
                default: break;
            }
        }
    }

    /* No status bits affected for this instruction */
}
