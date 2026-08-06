#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Bp instruction - CONTROL class
 *
 * BP - Breakpoint (Software breakpoint for debugging)
 *
 * Mnemonic: BP
 * Format: BP
 * Variants: 1
 * Operands: 0
 *
 * Opcode:
 *   0x0002 (BP) - Breakpoint instruction
 *
 * Operation:
 *   if BPT_enabled then
 *       Raise breakpoint trap (BPT)
 *   else
 *       Raise illegal instruction code trap (IIC)
 *   endif
 *
 * Description:
 *   Causes a breakpoint trap condition for program debugging. This instruction
 *   is specifically designed for use by debuggers and development tools to set
 *   software breakpoints in executing code.
 *
 *   When the BP instruction is executed, the CPU checks if the Breakpoint Trap
 *   (BPT) is enabled in the trap enable register. If enabled, a BPT trap is
 *   raised and the associated trap handler is invoked (typically a debugger).
 *   If BPT is not enabled, an Illegal Instruction Code (IIC) trap is raised
 *   instead.
 *
 *   The trap handler typically saves the CPU state, allows the debugger to
 *   inspect registers and memory, and provides facilities for single-stepping,
 *   continuing execution, or modifying program state.
 *
 * Trap Enable Check:
 *   The instruction checks the BPT bit in the trap enable register. In the
 *   ND-500 architecture, this is typically part of the Own Trap Enable (OTE)
 *   or Mother Trap Enable (MTE) registers in the domain system.
 *
 * Debugger Integration:
 *   Debuggers typically:
 *   1. Save the original instruction at the breakpoint location
 *   2. Replace it with a BP (0x0002) instruction
 *   3. When BP trap occurs, restore original instruction
 *   4. Allow user to inspect/modify state
 *   5. Optionally single-step or continue execution
 *
 * Flags: None modified
 *   All status flags remain unchanged by this instruction
 *
 * Trap conditions:
 *   - BPT (Breakpoint Trap) if breakpoint traps are enabled
 *   - IIC (Illegal Instruction Code) if breakpoint traps are disabled
 *
 * Performance:
 *   - Without trap: 1 cycle (if treated as NOP when disabled)
 *   - With trap: Variable (depends on trap handler complexity)
 *   - Typical trap overhead: 20-50 cycles
 *
 * Key Characteristics:
 *   - Zero operands (standalone instruction)
 *   - Intended for debugging only
 *   - Conditional trap based on BPT enable state
 *   - No architectural state modified (except via trap)
 *   - Opcode 0x0002 easily recognized by tools
 *   - Can be inserted/removed by debugger
 *   - Minimal code size (2 bytes)
 *
 * Common Use Cases:
 *   - Software breakpoints in debuggers
 *   - Conditional debugging (enable/disable BPT dynamically)
 *   - Assertion failures (when assertion fails, execute BP)
 *   - Debug hooks in production code (disabled normally)
 *   - Trace points for execution flow analysis
 *   - Test harness integration points
 *
 * Example Usage:
 *   ; Simple breakpoint
 *   BP                    ; Stop execution here for debugging
 *
 *   ; Conditional breakpoint (inserted by debugger)
 *   ORIGINAL_LOCATION:
 *       BP                ; Debugger replaced original instruction
 *       ; Debugger will restore original instruction when hit
 *
 *   ; Assertion failure
 *   COMP I1, EXPECTED_VALUE
 *   IF=GO:B ASSERT_OK
 *   BP                    ; Assertion failed - break to debugger
 *   ASSERT_OK:
 *       ; Continue execution
 *
 *   ; Debug hook (BPT disabled in production)
 *   START_CRITICAL_SECTION:
 *       BP                ; Only breaks if debugger enabled BPT
 *       ; Critical code
 *
 * Related Instructions:
 *   - NOOP: No operation (does nothing)
 *   - SOLO: Atomic operation sequence
 *   - INIT: Initialize domain (system control)
 *
 * Comparison with Related Instructions:
 *   - BP vs NOOP: BP may trap, NOOP never traps
 *   - BP vs SOLO: BP for debugging, SOLO for atomicity
 *   - BP vs software interrupt: BP is debug-specific, interrupts are general
 *
 * Implementation Notes:
 *   In a full implementation with trap system support, this instruction would:
 *   1. Check the BPT enable bit in the trap enable register
 *   2. If enabled, invoke the BPT trap handler
 *   3. If disabled, invoke the IIC trap handler
 *   4. Save appropriate context (PC, status) for trap handling
 *
 *   For emulator/debugger integration:
 *   - The emulator debugger can intercept BP instructions
 *   - Allows inspection of CPU state at breakpoint
 *   - Provides single-step and continue capabilities
 *
 * Trap Handler Expectations:
 *   The BPT trap handler typically:
 *   - Saves all CPU registers and status
 *   - Notifies debugger of breakpoint hit
 *   - Waits for debugger commands (inspect, modify, step, continue)
 *   - Restores CPU state when continuing
 *   - May replace BP with original instruction for single-step
 *
 * Security Considerations:
 *   - BP instructions in production code should have BPT disabled
 *   - Malicious code could use BP for timing attacks if BPT enabled
 *   - Debugger access should be restricted to authorized users
 *   - Production systems typically disable BPT entirely
 */
void nd500_instr_Bp(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 0) {
        printf("[ERROR] BP at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Manual ND-05.009.4, p.2086:
     *   "BreakPoint instruction Trap condition occurs when a breakpoint
     *    instruction (BP) is executed. If BPT is not enabled, a BP instruction
     *    will cause an IIC trap condition."
     * and p.2213 on IIC: "...or execution of a BP instruction with the BPT trap
     * disabled." Two outcomes, no third.
     *
     * This used to print a line and carry on, which is neither of them. That
     * mattered: NDIX runs asm("bp") in its PANIC path (machine/machdep.c:1028)
     * precisely so the trap will "take us back into trap() and save a context
     * block", and machine/trap.c:169 answers T_BPT with dumpsys() followed by a
     * reboot. With the instruction doing nothing, a panicking kernel fell
     * straight through that call and never took its crash dump.
     *
     * ND500X_BPDBG=1 brings the old log line back for anyone debugging the
     * instruction itself. It is off by default - a panicking guest is noisy
     * enough, and an unexplained "[BP] Breakpoint hit" in the middle of a
     * shutdown reads like an emulator fault when it is the guest saying it has
     * crashed. */
    {
        static int dbg = -1;
        if (dbg < 0) { const char* e = getenv("ND500X_BPDBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (dbg)
            printf("[BP] breakpoint instruction at PC=0x%08X (BPT %s)\n",
                   fi->address,
                   nd500_trap_is_enabled(cpu, TRAP_BPT) ? "enabled" : "disabled -> IIC");
    }

    if (nd500_trap_is_enabled(cpu, TRAP_BPT))
        raise_trap(cpu, TRAP_BPT, fi->address, 0);
    else
        trap_illegal_instruction(cpu, fi->address, fi->opcode);

    /* No status flags are modified by the BP instruction itself. */
}
