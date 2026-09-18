#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * E1Get instruction - MOVE class
 *
 * Store E1 Register (lower 32 bits): E1[31:0] -> <dest>
 *
 * Variants: 1
 * Mnemonics: e1=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFE3C
 *
 * Operation: regs.E1[31:0] -> <dest>
 *
 * Description:
 *   Reads the lower 32 bits of the E1 extended register and stores it to
 *   the destination operand as a word (32-bit). Updates Z and S flags
 *   based on the word value.
 *
 *   Note: E-registers are 64-bit but this instruction only transfers the
 *   lower 32 bits. Upper 32 bits remain unchanged in the register.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if lower 32 bits are zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/E1Get.cs
 */
void nd500_instr_E1Get(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] E1Get at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // en=: reads the E1 register (E = HIGH 32 bits of D1;
    // microcode STOREE1 @001174: A,E1). Was reading the low half (A1).
    uint32_t value = cpu->E[0];

    nd500_write_operand_word(cpu, &fi->operands[0], value);

    // Set Z and S flags based on the 32-bit word value
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

    // ST,SAVA: C and O cleared; K unchanged.
    cpu->ST1 &= ~(ND500_FLAG_C | ND500_FLAG_O);
}
