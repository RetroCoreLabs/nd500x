#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * AssignBaseRegTo instruction - MOVE class
 *
 * Load Local Base Register: <source> → B
 *
 * Variants: 1
 * Mnemonics: b:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFC08
 *
 * Operation: <source> → regs.B
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and stores it to the B
 *   (local base) register. Updates Z and S flags based on the value.
 *
 *   The B register is used as a base pointer for local variable access in
 *   subroutines and provides support for structured programming.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if value is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10.2
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/AssignBaseRegTo.cs
 */
void nd500_instr_AssignBaseRegTo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] AssignBaseRegTo at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_operand_word(cpu, &fi->operands[0]);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    cpu->B = value;

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
