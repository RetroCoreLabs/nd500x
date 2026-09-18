/*
 * Ret.c - ND-500 Ret instruction (CALL class)
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
#include "nd500_mmu.h"   /* nd500_mmu_translate - the DOMRET-BOGUS dump uses it */
#include <stdio.h>
#include <stdlib.h>
#include "nd500_settings.h"   /* emulator knobs, as plain fields */

/**
 * Ret instruction - CALL class
 *
 * Return from subroutine - Unwind stack frame and return to caller.
 * This is the standard return instruction paired with ENTS/ENTF/etc.
 *
 * Mnemonic: RET
 * Operands: 0
 * Opcode: 0x0080
 *
 * Stack Frame Layout (read from current B):
 *   +0   PREVB    Previous B register value
 *   +4   RETA     Return address
 *   +8   SP       Stack pointer
 *   +12  AUX      Auxiliary field
 *   +16  N        Argument count
 *   +20  ARG1+    Argument addresses
 *
 * Operation:
 *   1. Clear K flag
 *   2. Read B.PREVB and B.RETA from current stack frame
 *   3. Check for domain boundary (deferred to Phase 4)
 *   4. Normal return: restore PC from RETA, B from PREVB, L from RETA
 *   5. Check for stack underflow (PREVB == 0)
 *
 * Traps:
 *   - STU (Stack Underflow) if PREVB is zero
 *
 * Note: Domain switching logic (checking CAD != CED) is deferred to Phase 4.
 *
 * Reference: ND-500 Reference Manual and docs/instructions/asm/ (authoritative).
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Ret.cs
 */
void nd500_instr_Ret(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Stack frame field offsets */
    const uint32_t OFFSET_PREVB = 0;   /* Previous B */
    const uint32_t OFFSET_RETA  = 4;   /* Return address */

    /* Clear K flag (destination full flag) */
    nd500_clear_flag(cpu, ND500_FLAG_K);

    /* Read PREVB and RETA from current stack frame */
    uint32_t prev_b = nd500_read_memory_32(cpu, cpu->B + OFFSET_PREVB);
    uint32_t ret_addr = nd500_read_memory_32(cpu, cpu->B + OFFSET_RETA);

    /* Both reads come from the caller's frame, which lives on the USER STACK
     * and is therefore pageable. If either faults, raise_trap dispatches
     * synchronously - installing the handler PC and clearing the trap state -
     * and continuing would overwrite that handler PC (and B, and L) with
     * garbage read from an unmapped page. Same defect as JUMPG (dc2640c), and
     * RET is on one of the hottest paths in the machine.
     *
     * Aborting is restart-safe: RET re-reads the frame after the handler
     * RETTs, and it has committed nothing at this point - the only earlier
     * side effect is clearing the K flag, which RET does unconditionally on
     * every execution anyway. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Frame trace (env-gated) */
    if (nd500_settings()->framelog) {
        printf("[RET ] PC=0x%08X B=0x%08X read[B+0]=prevb=0x%08X read[B+4]=reta=0x%08X\n",
               fi->address, cpu->B, prev_b, ret_addr);
    }

    /*
     * CRITICAL: Domain boundary detection must happen BEFORE stack underflow check!
     *
     * From ND-500 Reference Manual Page 50:
     * "Control reverts to the calling domain when either the return address,
     *  the old base register, or both is zero when a return instruction is executed."
     *
     * "A return instruction with 0 in PREVB or RETA will only change domains
     *  if there is a domain to return to. If CAD is unequal to CED and non-zero,
     *  return is to the domain saved in the domain information table."
     *
     * CORRECT ORDER OF CHECKS:
     * 1. Check if (PREVB==0 OR RETA==0) AND (CAD != CED) AND (CAD != 0)
     *    -> This is a domain boundary, perform domain return
     * 2. ELSE IF PREVB == 0
     *    -> This is stack underflow (no domain to return to)
     * 3. ELSE
     *    -> Normal return within same domain
     *
     * BUG FIX (2025-01-15):
     * Previous implementation incorrectly checked stack underflow FIRST,
     * causing valid cross-domain returns to trap with STU (Stack Underflow).
     * The check order has been corrected to match ND-500 specification.
     *
     * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Ret.cs:72-105
     */

    /* Check for DOMAIN BOUNDARY first (before stack underflow) */
    if ((prev_b == 0 || ret_addr == 0) && (cpu->CAD != cpu->CED) && (cpu->CAD != 0)) {
        /*
         * DOMAIN BOUNDARY DETECTED - perform a cross-domain return.
         *
         * ND-500 Reference Manual (ND-05.009.4) section 4.2.5.2:
         *   "On return from a domain call, the registers CED, CAD, P and B are
         *    loaded from the old domain information table."
         *   "The memory management system will zeroize the return address and B
         *    register value in the domain information table at a domain call
         *    return to indicate that a call to the domain may be done."
         *
         * The "old" domain is the one we are returning FROM = the current CED.
         * Its DIT entry lives at DITBASE + CED*256 (PCBSIZ). The domain-call
         * save area (kernel struct pcb_call, verified against pcb.h and the
         * manual's octal offsets) is:
         *   call_ce (Calling Domain)            @ 128 (200B) 1 byte
         *   call_ca (Alternative of caller)     @ 129 (201B) 1 byte
         *   call_p  (P of caller)               @ 131 (203B) 4 bytes
         *   call_b  (B of caller)               @ 135 (207B) 4 bytes
         * DITBASE is a physical address, so use the physical bus accessors.
         *
         * TOS/THA/LL/HL loads from the NEW domain's DIT (also mandated by the
         * manual) are intentionally deferred until proven necessary by a fault.
         */
        uint32_t old_base = cpu->DITBASE + (uint32_t)cpu->CED * 256u;
        uint8_t  new_ced = nd500_bus_read8(cpu->machine, old_base + 128);
        uint8_t  new_cad = nd500_bus_read8(cpu->machine, old_base + 129);
        uint32_t new_p   = nd500_bus_read32(cpu->machine, old_base + 131);
        uint32_t new_b   = nd500_bus_read32(cpu->machine, old_base + 135);

        /* Diagnostic: a domain return whose saved P/B are ZERO means the DIT
         * call area was never (re)filled - the RET was NOT the kernel's
         * domain-call tail (locore 0x76E/0x76F) but some other frame whose
         * prevb/reta happened to be 0. Jumping to user P=0/B=0 from there is
         * catastrophic (crt0 runs with B=0). Log it loudly with the frame. */
        if (new_p == 0 && new_b == 0) {
            fprintf(stderr, "[DOMRET-BOGUS] ret@0x%08X CED=%u CAD=%u B=0x%08X prevb=0x%08X reta=0x%08X L=0x%08X -> call area EMPTY (P=0,B=0)\n",
                    fi->address, cpu->CED, cpu->CAD, cpu->B, prev_b, ret_addr, cpu->L);
            uint32_t fb = cpu->B;
            for (int lvl = 0; lvl < 8 && fb; lvl++) {
                uint32_t pb = nd500_read_memory_32(cpu, fb + 0);
                uint32_t ra = nd500_read_memory_32(cpu, fb + 4);
                fprintf(stderr, "[DOMRET-BOGUS]   frame L%d B=0x%08X prevb=0x%08X reta=0x%08X\n", lvl, fb, pb, ra);
                if (pb == fb) break;
                fb = pb;
            }
            { /* Which IMAGE is executing? Kernel text is loaded flat at
               * physical 0..a_text; user text is demand-paged elsewhere. The
               * physical address of the RET settles kernel-vs-user for good. */
              uint32_t pa = nd500_mmu_translate(cpu, fi->address, 0, 1);
              fprintf(stderr, "[DOMRET-BOGUS]   PC=0x%08X -> paddr=0x%08X byte=0x%02X (kernel text ends ~0x41A8C)\n",
                      fi->address, pa, nd500_bus_read8(cpu->machine, pa));
              extern void nd500_dump_pc_ring(const char*);
              nd500_dump_pc_ring("domret-bogus"); }
        }

        cpu->CED = new_ced;
        cpu->CAD = new_cad;
        cpu->PC  = new_p;
        cpu->L   = new_p;
        cpu->B   = new_b;

        /* Privilege follows the domain being entered (e.g. /etc/init in domain 1
         * runs non-privileged, pcb_pia=0), independent of the kernel's live PiA. */
        nd500_apply_domain_pia(cpu, new_ced);

        /* Zeroize return address (P) and B in the old DIT: marks the call as
         * completed so the domain may be called again (manual 4.2.5.2). */
        nd500_bus_write32(cpu->machine, old_base + 131, 0);
        nd500_bus_write32(cpu->machine, old_base + 135, 0);

        if (nd500_settings()->domdbg) {
            printf("[DOMRET] RET@0x%08X: domain %u -> %u  P=0x%08X B=0x%08X CAD=%u\n",
                   fi->address, new_cad /*caller alt was old CED path*/, new_ced,
                   new_p, new_b, new_cad);
        }
        return;
    }

    /* One-shot DIT call-area dump at the /etc/init launch RET (PC=0x29).
     * Verifies the real kernel DIT layout (256B/domain, call area at octal
     * 200B=128) is visible at DITBASE + CED*256. Env-gated ND500X_DITDBG. */
    if (nd500_settings()->ditdbg && fi->address == 0x29) {
        uint32_t base = cpu->DITBASE + (uint32_t)cpu->CED * 256u;
        printf("[DITDBG] PC=0x29 DITBASE=0x%08X CED=%u CAD=%u base=0x%08X\n",
               cpu->DITBASE, cpu->CED, cpu->CAD, base);
        printf("[DITDBG]   call_ce@128=%u call_ca@129=%u call_x@130=%u call_p@131=0x%08X call_b@135=0x%08X\n",
               nd500_bus_read8(cpu->machine, base + 128),
               nd500_bus_read8(cpu->machine, base + 129),
               nd500_bus_read8(cpu->machine, base + 130),
               nd500_bus_read32(cpu->machine, base + 131),
               nd500_bus_read32(cpu->machine, base + 135));
    }

    /* Check for STACK UNDERFLOW (after ruling out domain boundary) */
    if (prev_b == 0) {
        /*
         * Stack underflow detected.
         *
         * PREVB is zero AND we're not at a domain boundary (either CAD == CED,
         * or CAD == 0, meaning no calling domain exists). This indicates we're
         * trying to return past the outermost stack frame.
         *
         * This is an error condition - raise STU (Stack Underflow) trap.
         */
        ND500X_TRAPLOG("[TRAP] RET at PC=0x%08X: Stack underflow (PREVB=0, no domain to return to)\n",
               fi->address);
        trap_stack_underflow(cpu, fi->address);
        return;
    }

    /*
     * NORMAL RETURN - Same domain, no domain switching
     *
     * This is the typical case: returning from a subroutine within
     * the same domain. Restore the previous stack frame and jump
     * to the return address.
     */
    cpu->PC = ret_addr;      /* Jump to return address (B.RETA -> P) */
    cpu->L = ret_addr;       /* Update link register (B.RETA -> L) */
    cpu->B = prev_b;         /* Restore previous stack frame (B.PREVB -> B) */

    /* Stack frame automatically discarded (B now points to previous frame) */
}
