#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * LGet instruction - MOVE class
 *
 * Store L Register: L → <dest>
 *
 * Variants: 1
 * Mnemonics: l=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFDC0
 *
 * Operation: regs.L → <dest>
 *
 * Description:
 *   Reads the L (level) register and stores it to the destination operand
 *   as a word (32-bit). Updates Z and S flags based on the value.
 *
 *   The L register holds the current subroutine nesting level and is used
 *   for local variable addressing and stack frame management.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if L is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/LGet.cs
 */
void nd500_instr_LGet(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] LGet at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = cpu->L;

    /* Debug: trace L=: writes to help diagnose MMU issues */
    if (nd500_dbg_get_trace_mode()) {
        printf("[TRACE] LGet: L=0x%08X -> addr=0x%08X, mode=%d, MMU=%s\n",
               value, fi->operands[0].effective_address, fi->operands[0].mode,
               (cpu->machine && cpu->machine->mmu_enabled) ? "ON" : "OFF");
    }

    nd500_write_operand_word(cpu, &fi->operands[0], value);

    // Set Z and S flags based on the value
    if (value == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    if ((value & 0x80000000) != 0) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }
}
