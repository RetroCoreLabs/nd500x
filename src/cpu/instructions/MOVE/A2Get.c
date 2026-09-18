#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * A2Get instruction - MOVE class
 *
 * Store A2 Register: A2 -> <dest>
 *
 * Variants: 1
 * Mnemonics: a2=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFE39
 *
 * Operation: regs.A2 -> <dest>
 *
 * Description:
 *   Reads the A2 float register and stores it to the destination operand.
 *   Updates Z and S flags based on the register value.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if A2 is zero
 *   S = 1 if A2 sign bit is set
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 */
void nd500_instr_A2Get(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] A2Get at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_float_register(cpu, 2);
    nd500_write_operand_word(cpu, &fi->operands[0], value);

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
