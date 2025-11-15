#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * AssignRecordRegTo instruction - MOVE class
 *
 * Load Record Register: <source> → R
 *
 * Variants: 1
 * Mnemonics: r:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0x0018
 *
 * Operation: <source> → regs.R
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and stores it to the R
 *   (record base) register. Updates Z and S flags based on the value.
 *
 *   The R register is used as a base pointer for record/structure access and
 *   provides support for structured data types.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if value is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10.3
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/AssignRecordRegTo.cs
 */
void nd500_instr_AssignRecordRegTo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] AssignRecordRegTo at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_operand_word(cpu, &fi->operands[0]);
    cpu->R = value;

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
