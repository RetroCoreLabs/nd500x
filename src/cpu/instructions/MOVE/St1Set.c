#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * St1Set instruction - MOVE class
 *
 * Load ST1 Register: <source> → ST1
 *
 * Variants: 1
 * Mnemonics: st1:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFDB9
 *
 * Operation: <source> → regs.ST.ST1
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and stores it to the ST1
 *   (Status Register 1). Updates Z and S flags based on the value.
 *
 *   The ST1 register contains CPU status flags including Z (zero), S (sign),
 *   C (carry), and O (overflow) flags. This instruction allows programs to
 *   restore a previously saved processor status.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if value is zero
 *   S = 1 if bit 31 is set
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/St1Set.cs
 */
void nd500_instr_St1Set(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] St1Set at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_operand_word(cpu, &fi->operands[0]);
    cpu->ST1 = value;

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
