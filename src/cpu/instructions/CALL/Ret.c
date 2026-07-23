#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdlib.h>

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
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Ret.cs
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

    /* Frame trace (env-gated) */
    if (getenv("ND500X_FRAMELOG")) {
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
     *    → This is a domain boundary, perform domain return
     * 2. ELSE IF PREVB == 0
     *    → This is stack underflow (no domain to return to)
     * 3. ELSE
     *    → Normal return within same domain
     *
     * BUG FIX (2025-01-15):
     * Previous implementation incorrectly checked stack underflow FIRST,
     * causing valid cross-domain returns to trap with STU (Stack Underflow).
     * The check order has been corrected to match ND-500 specification.
     *
     * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Ret.cs:72-105
     */

    /* Check for DOMAIN BOUNDARY first (before stack underflow) */
    if ((prev_b == 0 || ret_addr == 0) && (cpu->CAD != cpu->CED) && (cpu->CAD != 0)) {
        /*
         * DOMAIN BOUNDARY DETECTED
         *
         * This is a cross-domain return. The zero values in PREVB/RETA
         * indicate we're returning from a called domain back to the caller.
         *
         * Domain return requires:
         * 1. Load caller context from PCB.pcb_call structure
         * 2. Save current domain state to DIT (Domain Information Table)
         * 3. Restore calling domain state from DIT
         * 4. Update CAD and CED to calling domain
         * 5. Restore B and P from saved context
         *
         * NOTE: Domain switching is deferred to Phase 4.
         * For now, we treat this as an error to avoid incorrect behavior.
         */
        printf("[TODO] RET at PC=0x%08X: Domain return from domain %u to %u not yet implemented\n",
               fi->address, cpu->CED, cpu->CAD);
        printf("       PREVB=0x%08X, RETA=0x%08X (domain boundary detected)\n",
               prev_b, ret_addr);

        /* TODO: Call nd500_domain_return(cpu) when domain switching is implemented */
        /* For now, we cannot safely proceed, so we trap */
        trap_stack_underflow(cpu, fi->address);
        return;
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
        printf("[TRAP] RET at PC=0x%08X: Stack underflow (PREVB=0, no domain to return to)\n",
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
    cpu->PC = ret_addr;      /* Jump to return address (B.RETA → P) */
    cpu->L = ret_addr;       /* Update link register (B.RETA → L) */
    cpu->B = prev_b;         /* Restore previous stack frame (B.PREVB → B) */

    /* Stack frame automatically discarded (B now points to previous frame) */
}
