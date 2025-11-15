#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * ThaGet instruction - MOVE class
 *
 * Store THA Register: THA → <dest>
 *
 * Variants: 1
 * Mnemonics: tha=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFDCB
 *
 * Operation: regs.THA → <dest>
 *
 * Description:
 *   Reads the THA (Trap Handler Address) register and stores it to the
 *   destination operand as a word (32-bit). Updates Z and S flags based
 *   on the value.
 *
 *   The THA register contains the address of the current trap handler
 *   and is used for exception and interrupt handling.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if THA is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/ThaGet.cs
 */
void nd500_instr_ThaGet(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] ThaGet at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = cpu->THA;
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
