#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Clrk instruction - MOVE class
 *
 * Clear K Flag: 0 → K
 *
 * Variants: 1
 * Mnemonics: clrk
 * Operands: 0 (no operands)
 *
 * Opcode: 0xFE03
 *
 * Operation: 0 → ST.K
 *
 * Description:
 *   Clears the K (Destination Full) status bit in the status register.
 *   The K flag is used for signaling and synchronization purposes, typically
 *   to indicate when a destination buffer or register is full.
 *
 * Flags: K (Destination Full)
 *   K = 0 (always cleared)
 *
 * Trap conditions:
 *   - None
 *
 * Reference: ND-500 Reference Manual
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Clrk.cs
 */
void nd500_instr_Clrk(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Clear K flag in status register
    cpu->ST1 &= ~ND500_FLAG_K;
}
