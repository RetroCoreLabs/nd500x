#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * E1Set instruction - MOVE class
 *
 * Load E1 Register (lower 32 bits): <source> → E1[31:0]
 *
 * Variants: 1
 * Mnemonics: e1:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFE34
 *
 * Operation: <source> → regs.E1[31:0]
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and stores it to the lower
 *   32 bits of the E1 extended register. Updates Z and S flags based on
 *   the word value.
 *
 *   Note: E-registers are 64-bit but this instruction only transfers to the
 *   lower 32 bits. Upper 32 bits remain unchanged.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if value is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/E1Set.cs
 */
void nd500_instr_E1Set(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] E1Set at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_operand_word(cpu, &fi->operands[0]);

    // Read current E1, preserve upper 32 bits, replace lower 32 bits
    uint64_t e1_full = nd500_read_double_register(cpu, 1);
    e1_full = (e1_full & 0xFFFFFFFF00000000ULL) | (uint64_t)value;
    nd500_write_double_register(cpu, 1, e1_full);

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
}
