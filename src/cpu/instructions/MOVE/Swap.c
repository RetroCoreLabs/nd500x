#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Swap instruction - MOVE class
 *
 * Exchange Contents of Two Operands: <op1> ⇄ <op2>
 *
 * Variants: 6
 * Mnemonics: swap
 * Operands: 2 (<op1/rw/t>, <op2/rw/t>)
 *
 * Opcodes:
 *   0xFCBD (BI swap - bit)
 *   0xFCBE (BY swap - byte)
 *   0xFCBF (H swap - halfword)
 *   0x0052 (W swap - word)
 *   0xFCDC (F swap - float)
 *   0xFCDD (D swap - double)
 *
 * Operation: <op1> ⇄ <op2>
 *
 * Description:
 *   The contents of the first operand are stored in the second operand, and
 *   the original contents of the second operand are stored in the first.
 *   The operands are assumed to have the same data type.
 *
 *   This instruction is useful for register/variable exchanges without requiring
 *   a temporary storage location.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if original <op1> is zero
 *   S = 1 if original <op1> sign bit is set
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10.8
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Swap.cs
 */
void nd500_instr_Swap(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] Swap at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Determine data type based on opcode
    Nd500DataType dtype;
    switch (fi->opcode) {
        case 0xFCBD: dtype = ND500_DTYPE_BIT; break;         // BI swap - swaps single bits
        case 0xFCBE: dtype = ND500_DTYPE_BYTE; break;        // BY swap
        case 0xFCBF: dtype = ND500_DTYPE_HALFWORD; break;    // H swap
        case 0x0052: dtype = ND500_DTYPE_WORD; break;        // W swap
        case 0xFCDC: dtype = ND500_DTYPE_WORD; break;        // F swap (32-bit float)
        case 0xFCDD: dtype = ND500_DTYPE_DOUBLEWORD; break;  // D swap (64-bit double)
        default:
            printf("[ERROR] Swap at PC=0x%08X: Unknown opcode 0x%04X\n",
                   fi->address, fi->opcode);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    // Read both operands
    uint64_t value1 = nd500_read_operand_value(cpu, &fi->operands[0], dtype);
    uint64_t value2 = nd500_read_operand_value(cpu, &fi->operands[1], dtype);

    // Write swapped values
    nd500_write_operand_value(cpu, &fi->operands[0], value2, dtype);
    nd500_write_operand_value(cpu, &fi->operands[1], value1, dtype);

    // Set Z and S flags based on original contents of op1
    nd500_set_flags_zs(cpu, value1, dtype);
}
