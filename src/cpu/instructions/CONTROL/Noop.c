#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Noop instruction - CONTROL class
 *
 * No Operation: Does Nothing
 *
 * Variants: 1
 * Mnemonics: noop
 * Operands: 0 (no operands)
 *
 * Opcode: 0x0003
 *
 * Operation: None
 *
 * Description:
 *   Does absolutely nothing. Useful for:
 *   - Deleting code without removing space
 *   - Leaving space for future modifications
 *   - Padding/alignment
 *   - Timing delays
 *   - Breakpoints in debugging
 *
 *   The PC will be advanced automatically by cpu_step() after execution.
 *
 * Flags: None affected
 *
 * Trap conditions:
 *   - None
 *
 * Reference: ND-500 Reference Manual, Chapter 15.10
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CONTROL/Noop.cs
 */
void nd500_instr_Noop(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Do absolutely nothing - that's the point!
    // PC is advanced automatically by cpu_step()
    (void)cpu;  // Suppress unused parameter warning
    (void)fi;   // Suppress unused parameter warning
}
