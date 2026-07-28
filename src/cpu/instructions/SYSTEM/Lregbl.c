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

    bool privileged = (cpu->ST1 & ND500_FLAG_PIA) != 0;

    /* Load registers based on mask bits (like C# lines 68-122).
     * The mask operand is a 32-bit word, so only bits 0:31 (reg_num 1..32,
     * up to CTE1) are addressable; shifting by 32 or more would be undefined
     * behaviour, so the loop is capped at reg_num 32. Registers 33..37
     * (CTE2/MTE1/MTE2/TEMM1/TEMM2) are not reachable by a 32-bit mask.
     *
     * Address calculation: <address> + register_number*4
     *
     * TWO-PHASE (read-all-then-apply) is REQUIRED for correctness: the block
     * reads translate through the MMU using the CURRENT domain (cpu->CED). If we
     * applied each register the moment we read it, loading register 24 (CED)
     * would switch the addressing domain MID-INSTRUCTION, so the very next slot
     * (register 25 = CAD) would be translated under the NEW domain. In NDIX the
     * register block lives in the kernel u-area (segment 29, e.g. 0xE800xxxx),
     * which is accessible under the kernel domain (0) but NOT under the restored
     * user domain (1): the CAD read then protect-faults at base+96. On real
     * ND-500 lregbl is atomic - CED/CAD take architectural effect only at the
     * instruction boundary - so all reads must use one consistent domain. We read
     * every selected register value first (domain unchanged), then apply. */
    uint32_t vals[33];
    bool     has[33] = { false };
    for (int reg_num = 1; reg_num <= 32; reg_num++) {
        if ((mask & (1u << (reg_num - 1))) != 0) {
            /* Privilege restriction (manual 16.27.2): in non-privileged mode
             * the mask is reduced to registers modifiable outside privileged
             * mode. Skip ST2 (18), PS (19), CED (24), CAD (25) and the trap
             * control registers CTE1 (32) [and CTE2/MTE/TEMM, unreachable via
             * a 32-bit mask]. CTE1 at bit 31 previously escaped this filter. */
            if (!privileged) {
                if (reg_num == 18 || reg_num == 19 || reg_num == 24 ||
                    reg_num == 25 || reg_num == 32) {
                    continue;
                }
            }
            /* Register N is stored at address + (N-1)*4: the base `address`
             * operand points at the FIRST selected register (reg 1 = P) itself,
             * i.e. reg 1 is at offset 0. (Manual ND-05.009.4 Fig.2 register
             * block: P,L,B,R,I1..; NDIX passes address = the arg2/P slot of the
             * ENTT frame, so reg 1 must read from base+0, not base+4.) sregbl
             * uses the identical convention so the save/load pair stays consistent. */
            uint32_t reg_address = address + (uint32_t)((reg_num - 1) * 4);
            vals[reg_num] = nd500_read_memory_32(cpu, reg_address);
            if (nd500_trap_occurred()) return;  /* block read faulted - abort */
            has[reg_num] = true;
        }
    }

    /* PHASE 2: apply the buffered values now that all block reads are done. */
    for (int reg_num = 1; reg_num <= 32; reg_num++) {
        if (has[reg_num]) {
            uint32_t reg_value = vals[reg_num];

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

    /* NDIX returns from a trap handler with `lregbl $CNTXMASK,r3` (machine/
     * locore.c trapex, CNTXMASK=0x1C3FFFF) rather than RETT: the block reloads
     * P (reg 1), ST1 (with PiA), CED and CAD - the architectural trap-return.
     * RETT is what normally clears the emulator's in_trap_handler guard (and
     * trap_cross_domain); since NDIX never executes RETT for kernel traps, an
     * lregbl that reloads the program counter WHILE a handler is active IS the
     * handler return. Clear the guard here, otherwise it stays set and the
     * returned-to program's next legitimate page fault is misdetected as a
     * double fault and halts (observed: init's PC=8 stack write PGF). */
    if (has[1] && cpu->in_trap_handler) {
        cpu->in_trap_handler = false;
    }

    /* PiA is a DOMAIN attribute (see nd500_apply_domain_pia / cpu.c), not a
     * freely-saved status bit: it must follow the executing domain across a
     * domain transition, and an lregbl trap-return that reloads P + CED IS such
     * a transition. Reloading ST1 (reg 17) from the saved block can restore a
     * STALE PiA - e.g. a kernel fu/su probe (fubyte@0x675) faults while its
     * saved context block carries PiA=0; lregbl-returning into domain 0 (the
     * privileged kernel) with that stale PiA=0 then makes entrap's next
     * privileged instruction (tutti/dcc) trap IIC -> double fault. Reapply the
     * restored domain's PiA so privilege matches the domain being resumed. This
     * is a no-op without a real DIT (single-domain SINTRAN) and a no-op when the
     * saved PiA already matched the domain (the normal kernel-trap return). */
    if (has[1] && cpu->DITBASE) {
        extern void nd500_apply_domain_pia(Nd500Cpu* cpu, uint32_t domain);
        nd500_apply_domain_pia(cpu, cpu->CED);
    }

    /* No status bits affected for this instruction */
}
