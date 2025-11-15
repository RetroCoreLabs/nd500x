#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Ote2Set instruction - MOVE class
 *
 * Load OTE2 Register: <source> → OTE2
 *
 * Variants: 1
 * Mnemonics: ote2:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFDBC
 *
 * Operation: <source> → regs.OTE2
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and stores it to the OTE2
 *   (Object Table Entry 2) register. Updates Z and S flags based on the value.
 *
 *   The OTE registers contain object table entry information used for
 *   object-oriented programming support and descriptor management.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if value is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Ote2Set.cs
 */
void nd500_instr_Ote2Set(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] Ote2Set at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_operand_word(cpu, &fi->operands[0]);
    cpu->OTE2 = value;

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
