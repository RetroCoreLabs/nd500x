#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * E3Set instruction - MOVE class
 *
 * Load E3 Register (lower 32 bits): <source> → E3[31:0]
 *
 * Variants: 1
 * Mnemonics: e3:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFE36
 *
 * Operation: <source> → regs.E3[31:0]
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and stores it to the lower
 *   32 bits of the E3 extended register. Updates Z and S flags based on
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
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/E3Set.cs
 */
void nd500_instr_E3Set(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] E3Set at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_operand_word(cpu, &fi->operands[0]);

    // Read current E3, preserve upper 32 bits, replace lower 32 bits
    uint64_t e3_full = nd500_read_double_register(cpu, 3);
    e3_full = (e3_full & 0xFFFFFFFF00000000ULL) | (uint64_t)value;
    nd500_write_double_register(cpu, 3, e3_full);

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
