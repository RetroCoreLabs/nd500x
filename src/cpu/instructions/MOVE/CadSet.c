#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * CadSet instruction - MOVE class
 *
 * Load CAD Register: <source> → CAD
 *
 * Variants: 1
 * Mnemonics: cad:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFDBA
 *
 * Operation: <source> → regs.CAD
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and stores it to the CAD
 *   (Current Address Descriptor) register. Updates Z and S flags based on
 *   the value.
 *
 *   The CAD register contains the current address descriptor used for
 *   memory management and address translation. Modifying this register
 *   changes the active memory mapping context.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if value is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/CadSet.cs
 */
void nd500_instr_CadSet(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] CadSet at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_operand_word(cpu, &fi->operands[0]);
    cpu->CAD = value;

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
