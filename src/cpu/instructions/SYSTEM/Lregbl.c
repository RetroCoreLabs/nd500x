/*
 * Lregbl.c - ND-500 Lregbl instruction (SYSTEM class)
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
 * Reference: ND-500 Reference Manual, Chapter 16.27.2
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Lregbl.cs
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
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    bool privileged = (cpu->ST1 & ND500_FLAG_PIA) != 0;

    /* TWO-PHASE (read-all-then-apply) is REQUIRED for correctness: the block
     * reads translate through the MMU using the CURRENT domain (cpu->CED). If
     * each register were applied the moment it is read, loading CED would
     * switch the addressing domain MID-INSTRUCTION, so the next slot (CAD)
     * would be translated under the NEW domain. In NDIX the register block
     * lives in the kernel u-area (segment 29, e.g. 0xE800xxxx), which is
     * accessible under the kernel domain (0) but NOT under the restored user
     * domain (1): the CAD read then protect-faults. On real ND-500 lregbl is
     * atomic - CED/CAD take effect only at the instruction boundary - so all
     * reads use one consistent domain: read every selected word, then apply.
     *
     * Non-privileged (manual 16.27.2): the mask is reduced to the registers an
     * assembly instruction may change there - not ST2, PS, CED, CAD, CTE, MTE
     * or TEMM (words 17, 18, 23, 24, 31-36). */
    uint32_t vals[37];
    bool     has[37] = { false };
    for (unsigned bit = 0; bit < ND500_REGBLOCK_MASK_BITS; bit++) {
        int words[2];
        int n = (mask >> bit) & 1u ? nd500_regblock_words(bit, words) : 0;
        for (int k = 0; k < n; k++) {
            int w = words[k];
            if (!privileged && (w == 17 || w == 18 || w == 23 || w == 24 || (w >= 31 && w <= 36))) {
                continue;
            }
            if (!nd500_regblock_register(cpu, w)) {
                continue;                /* MIC is not emulated */
            }
            vals[w] = nd500_read_memory_32(cpu, address + (uint32_t)w * 4u);
            if (nd500_trap_occurred()) return;  /* block read faulted - abort */
            has[w] = true;
        }
    }

    /* PHASE 2: apply the buffered values now that all block reads are done. */
    for (int w = 0; w < 37; w++) {
        if (has[w]) {
            *nd500_regblock_register(cpu, w) = vals[w];
        }
    }

    /* NDIX returns from a trap handler with `lregbl $CNTXMASK,r3` (machine/
     * locore.c trapex, CNTXMASK=0x1C3FFFF) rather than RETT: the block reloads
     * P, STS (ST1 with PiA, ST2), PS, CED, CAD and MIC - the trap return.
     * RETT is what normally clears the emulator's in_trap_handler guard (and
     * trap_cross_domain); since NDIX never executes RETT for kernel traps, an
     * lregbl that reloads the program counter WHILE a handler is active IS the
     * handler return. Clear the guard here, otherwise it stays set and the
     * returned-to program's next legitimate page fault is misdetected as a
     * double fault and halts (observed: init's PC=8 stack write PGF). */
    if (has[0] && cpu->in_trap_handler) {
        cpu->in_trap_handler = false;
        /* This IS the trap return, so it is also where the CALL/ENT* sequence
         * interlock saved by the handler's ENTT must come back. Without it the
         * resumed program's pending CALL is gone and its retried ENTS raises a
         * false ISE - which is how vi died after a page fault on its own ENTS. */
        extern void nd500_trap_seq_pop_top(Nd500Cpu* cpu);
        nd500_trap_seq_pop_top(cpu);
    }

    /* PiA is a DOMAIN attribute (see nd500_apply_domain_pia / cpu.c), not a
     * freely-saved status bit: it must follow the executing domain across a
     * domain transition, and an lregbl trap-return that reloads P + CED IS such
     * a transition. Reloading ST1 (mask bit 16) from the saved block can restore a
     * STALE PiA - e.g. a kernel fu/su probe (fubyte@0x675) faults while its
     * saved context block carries PiA=0; lregbl-returning into domain 0 (the
     * privileged kernel) with that stale PiA=0 then makes entrap's next
     * privileged instruction (tutti/dcc) trap IIC -> double fault. Reapply the
     * restored domain's PiA so privilege matches the domain being resumed. This
     * is a no-op without a real DIT (single-domain SINTRAN) and a no-op when the
     * saved PiA already matched the domain (the normal kernel-trap return). */
    if (has[0] && cpu->DITBASE) {
        extern void nd500_apply_domain_pia(Nd500Cpu* cpu, uint32_t domain);
        nd500_apply_domain_pia(cpu, cpu->CED);
    }

    /* No status bits affected for this instruction */
}
