/*
 * nd500_indirect.c - indirect segment handling
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Implements cross-domain calls via indirect segment capabilities.
 * Segment 31 is reserved for SINTRAN monitor calls (MON).
 *
 * Reference: ND500_PCB_MMU_COMPLETE_REFERENCE.md
 * C# Reference: CpuND500.IndirectSegments.cs
 */

#include "nd500_indirect.h"
#include "nd500_mmu.h"
#include "instruction_helpers.h"
#include "cpu_protos.h"   /* nd500_quiet */
#include "../machine/machine_protos.h"
#include "nd500_mon_sintran.h"   /* SINTRAN MON seam - see that header */
#include "nd500_fecall.h"        /* nd500_fecall, MON 600 */
#include <stdio.h>
#include <stdlib.h>   /* getenv */
#include <string.h>
#include "nd500_settings.h"   /* emulator knobs, as plain fields */

/* =========================================================================
 * INTERNAL CONSTANTS
 * ========================================================================= */

/* Segment 31 is reserved for SINTRAN MON calls */
#define SINTRAN_SEGMENT 31


/* =========================================================================
 * INDIRECT SEGMENT CHECK AND HANDLING
 * ========================================================================= */

int nd500_check_indirect_call(
    Nd500Cpu* cpu,
    uint32_t target_addr,
    uint32_t arg_count,
    const uint32_t* arg_addresses,
    uint32_t instruction_addr,
    uint32_t* out_resolved)
{
    if (!cpu || !out_resolved) {
        return INDIRECT_ERROR;
    }

    /* Extract segment number from address (bits 31-27) */
    uint32_t segment = (target_addr >> 27) & 0x1F;
    uint32_t offset = target_addr & 0x07FFFFFF;

    /* If MMU is disabled, all calls are direct */
    if (!cpu->machine || !cpu->machine->mmu_enabled) {
        *out_resolved = target_addr;
        return INDIRECT_DIRECT;
    }

    /* Get program capability for this segment in the current executing domain.
     * Must be the ACTIVE capability (guest DIT memory when guest-table routing
     * applies): a fork child's fresh domain exists only in the kernel-written
     * DIT, and the shadow-table lookup returned 0 there, sending the child's
     * first syscall gate into the SINTRAN MON path (bogus MON LEAVE halt). */
    uint16_t pc = nd500_mmu_get_active_program_capability(cpu, cpu->CED, segment);

    /* Check PC_IND flag (bit 15) - if clear, this is a direct call.
     * EXCEPTION: a CALLG into the segment-31 SINTRAN window is ALWAYS a monitor
     * call, whatever the program capability says - the emulator has no real
     * SINTRAN code at segment 31 to jump to, so it must be serviced as a MON
     * dispatch (segment-31 block below). Without this, a MON 600 fecall whose
     * seg-31 capability lacks PC_IND (e.g. the executing domain/CED never had it
     * installed, or mmusetup's OMC/ND-100 capability is what's active) falls
     * through to direct-call entry-point validation and traps "not an entry
     * point" at the fecall. [MON 600 fix] */
    if ((pc & PC_IND) == 0 && segment != SINTRAN_SEGMENT) {
        *out_resolved = target_addr;
        return INDIRECT_DIRECT;
    }

    /* =======================================================================
     * INDIRECT CALL - Extract target domain and segment from capability
     * ======================================================================= */

    /* Extract target domain (bits 13-5, 9 bits) and segment (bits 4-0, 5 bits) */
    uint32_t target_domain = (pc & PC_DOM) >> 5;
    uint32_t target_segment = pc & PC_SEG;

    /* =======================================================================
     * SINTRAN SEGMENT 31 - MON CALL HANDLING
     *
     * Segment 31 is reserved for SINTRAN monitor calls.
     * The offset IS the MON number (not divided by 4).
     * ======================================================================= */

    /* A CALLG *into* the segment-31 window is a SINTRAN monitor call by
     * architecture, regardless of what the (possibly OMC/ND-100-configured)
     * capability resolves target_segment to. The NDIX kernel's MON 600 fecall is
     * CALLG 0xF8000180 (segment 31, offset 0x180 = 384 = MON 600 octal); with
     * mmusetup pointing segment 31 at "domain 0 segment 1 + OMC", target_segment
     * came out 1, so this MON dispatch was skipped and the call fell into the
     * unimplemented domain-switch branch. Gate on the SOURCE segment too so the
     * monitor call dispatches (the offset is the MON number). [NDIX MON 600 fix] */
    /* Diagnostic: dump the seg-31 capability so we can distinguish a SINTRAN MON
     * gate (PC_OMC set = "other machine" / ND-100) from a genuine NDIX cross-domain
     * syscall gate (PC_IND, no PC_OMC -> target_domain 0 = kernel). Env ND500X_INDDBG. */
    {
        static int inddbg = -1;
        if (inddbg < 0) inddbg = nd500_settings()->inddbg;
        if (inddbg && (segment == SINTRAN_SEGMENT || target_segment == SINTRAN_SEGMENT)) {
            fprintf(stderr, "[INDDBG] seg=%u off=0x%X CED=%u cap=0x%04X PC_IND=%d PC_OMC=%d tgt_dom=%u tgt_seg=%u\n",
                    segment, offset, cpu->CED, pc, (pc & PC_IND) ? 1 : 0, (pc & PC_OMC) ? 1 : 0,
                    target_domain, target_segment);
            /* For a genuine ND-500 cross-domain gate (PC_IND, no PC_OMC): the offset
             * is a routine index into the Start Address Vector at addr 0 of the target
             * segment in the target domain. SAV[0]=length, entry[N] at (N+1)*4. Dump the
             * SAV header + the resolved entry so we can confirm it points at _domain_call. */
            if ((pc & PC_IND) && !(pc & PC_OMC)) {
                uint32_t sav_base = (uint32_t)target_segment << 27;   /* addr 0 of target seg */
                uint32_t len   = nd500_read_memory_32_domain(cpu, sav_base + 0, (uint8_t)target_domain);
                uint32_t entry = nd500_read_memory_32_domain(cpu, sav_base + (offset + 1) * 4, (uint8_t)target_domain);
                fprintf(stderr, "[INDDBG]   SAV@dom%u:0x%08X len=0x%08X entry[%u]@0x%08X=0x%08X\n",
                        target_domain, sav_base, len, offset, sav_base + (offset + 1) * 4, entry);
            }
        }
    }

    /* =======================================================================
     * NDIX CROSS-DOMAIN SYSCALL CALL
     *
     * A seg-31 gate with PC_IND but WITHOUT PC_OMC is a genuine ND-500 cross-domain
     * call into another ND-500 domain (the NDIX kernel), NOT a SINTRAN/ND-100 MON
     * call (those have PC_OMC set = "other machine"). init's execve does
     * `call $0xF8000000` (offset 0) whose seg-31 capability is PC_IND|1 (cap 0x8001)
     * -> target domain 0 (kernel), segment 1. Resolve the entry via the target
     * segment's Start Address Vector (SAV at addr 0 of the target segment: SAV[0]=len,
     * routine #N at (N+1)*4; the offset is the routine index) and perform the domain
     * switch - the inverse of the Ret.c domain-return. Gated on a real kernel DIT
     * (cpu->DITBASE) so single-domain SINTRAN (no DIT) is untouched. [NDIX syscall]
     * ======================================================================= */
    if ((pc & PC_IND) && !(pc & PC_OMC) && cpu->DITBASE &&
        target_segment != SINTRAN_SEGMENT) {
        uint32_t sav_base = (uint32_t)target_segment << 27;   /* addr 0 of target seg */
        uint32_t sav_len  = nd500_read_memory_32_domain(cpu, sav_base, (uint8_t)target_domain);
        uint32_t entry    = nd500_read_memory_32_domain(cpu, sav_base + (offset + 1) * 4, (uint8_t)target_domain);

        /* Sanity: a valid SAV has a non-zero length and a plausible entry. If not,
         * fall through to the SINTRAN MON path (keeps prior behaviour on garbage). */
        if (sav_len != 0 && offset < sav_len && entry != 0) {
            /* Save the CALLER's context into the TARGET domain's DIT call-area so a
             * later RET domain-return (Ret.c) returns to the caller; the kernel's
             * _domain_call also reads the caller P/B from here (_pcbtab+ALT_P/ALT_B =
             * call_p@131/call_b@135). Physical DIT accessors (DITBASE is physical). */
            uint32_t db = cpu->DITBASE + (uint32_t)target_domain * 256u;
            nd500_bus_write8 (cpu->machine, db + 128, (uint8_t)cpu->CED);              /* call_ce */
            nd500_bus_write8 (cpu->machine, db + 129, (uint8_t)cpu->CAD);              /* call_ca */
            nd500_bus_write32(cpu->machine, db + 131, cpu->pending_call_return_address);/* call_p */
            nd500_bus_write32(cpu->machine, db + 135, cpu->B);                          /* call_b */

            /* Switch domains: CED <- kernel (target), CAD <- caller domain. Privilege
             * follows the entered domain (kernel pcb_pia=1). Do NOT set PC/L or clear
             * the pending-call state here: we return INDIRECT_DIRECT so Callg validates
             * the entry opcode and jumps, and the entry (_domain_call) begins with an
             * ENTM which REQUIRES pending_call_return_address != 0 (else it raises ISE).
             * The entry is a seg-1 logical addr; the dom0 seg-1 alias maps it to the
             * kernel code; ENTM then sets B=_Kstack (above _Ktrap, so no stack
             * underflow). The caller's return P/B are already saved in the DIT call
             * area for the eventual domain-return (Ret.c). */
            uint32_t caller_ced = cpu->CED;
            cpu->CAD = caller_ced;
            cpu->CED = target_domain;
            nd500_apply_domain_pia(cpu, target_domain);

            /* Mark the domain boundary: a cross-domain call starts a FRESH frame
             * chain in the entered domain, so the entry's ENTM (entm _Kstack) must
             * record prev_b = 0 in the kernel's bottom frame. ENTM sets frame.prev_b
             * to the OLD B, so clear B here; otherwise prev_b = the caller's B (non-
             * zero) and the kernel's final `ret` is taken as a NORMAL return (to the
             * frame RETA in the WRONG domain, PC=0x1E in CED=0) instead of the
             * domain-return that Ret.c performs when prev_b==0 && CAD!=CED. [dom call] */
            cpu->B = 0;

            if (nd500_settings()->inddbg)
                fprintf(stderr, "[SYSCALL] cross-domain CALL caller_dom=%u -> dom%u entry=0x%08X (retP=0x%08X B=0x%08X)\n",
                        caller_ced, target_domain, entry, cpu->pending_call_return_address, cpu->B);

            *out_resolved = entry;
            return INDIRECT_DIRECT;
        }
    }

    if (segment == SINTRAN_SEGMENT || target_segment == SINTRAN_SEGMENT) {
        /* MON number is the offset directly (from assembly analysis) */
        uint32_t mon_number = offset;

        /* MON 600 (octal, offset 0x180) is the NDIX ND-100 front-end call
         * (fecall) - disk/console/init I/O. It needs full cpu/machine/DMA access,
         * so it is serviced directly, not through the generic MON registry. */
        if (mon_number == 0x180) {
            nd500_fecall(cpu, arg_count, arg_addresses);
            *out_resolved = cpu->pending_call_return_address;
            return INDIRECT_HANDLED;
        }

        /* Everything that is not MON 600 is SINTRAN III monitor-call
         * emulation, which lives behind the seam in nd500_mon_sintran.h so
         * that this file - and therefore all of nd500_cpu - carries no
         * ndmonlib dependency. In a build configured with
         * ND500X_WITH_NDMON=OFF the stub implementation is linked and this
         * stops the machine with an explanatory message. */
        return nd500_mon_sintran_call(cpu, mon_number, instruction_addr,
                                      arg_count, arg_addresses, out_resolved);
    }

    /* =======================================================================
     * OTHER INDIRECT SEGMENTS - DOMAIN SWITCHING
     *
     * For non-SINTRAN indirect segments, we need to:
     * 1. Read the Start Address Vector from target segment
     * 2. Prepare domain switch context
     * 3. Return entry point address
     *
     * This is not yet implemented - just log and continue.
     * ======================================================================= */

    printf("[INDIRECT] Domain switch not implemented: "
           "segment %u -> domain %u segment %u (offset 0x%08X)\n",
           segment, target_domain, target_segment, offset);

    /* For now, treat as direct call (will likely fail, but allows debugging) */
    *out_resolved = target_addr;
    return INDIRECT_DIRECT;
}

/* =========================================================================
 * SEGMENT 31 SETUP
 *
 * Called during DOM/SEG loading to configure segment 31 for SINTRAN calls.
 * ========================================================================= */

void nd500_setup_sintran_segment(Nd500Cpu* cpu, uint8_t domain) {
    if (!cpu) return;

    /* Setup PC[31] as indirect segment pointing to domain 0, segment 31
     * PC_IND (0x8000) | domain 0 in bits 13-5 | segment 31 in bits 4-0
     * = 0x8000 | (0 << 5) | 31 = 0x801F */
    uint16_t capability = PC_IND | (0 << 5) | SINTRAN_SEGMENT;

    nd500_mmu_set_program_capability(cpu, domain, SINTRAN_SEGMENT, capability);
}

/* =========================================================================
 * HELPER FUNCTIONS
 * ========================================================================= */

int nd500_program_mmu_enabled(Nd500Cpu* cpu) {
    if (!cpu || !cpu->machine) return 0;
    return cpu->machine->mmu_enabled;
}
