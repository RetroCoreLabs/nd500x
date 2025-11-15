#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

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

    /* TODO (Phase 4): Check for domain boundary
     * If (prev_b == 0 || ret_addr == 0) && (CAD != CED) && (CAD != 0):
     *     Perform domain return
     * For now, we skip domain switching.
     */

    /* Normal return - same domain */
    if (prev_b == 0) {
        /* Stack underflow - returning past outermost frame */
        printf("[TRAP] RET at PC=0x%08X: Stack underflow (PREVB=0)\n",
               fi->address);
        trap_stack_underflow(cpu, fi->address);
        return;
    }

    /* Restore registers */
    cpu->PC = ret_addr;      /* Jump to return address */
    cpu->L = ret_addr;       /* Update link register */
    cpu->B = prev_b;         /* Restore previous stack frame */

    /* Stack frame automatically discarded (B now points to previous frame) */
}
