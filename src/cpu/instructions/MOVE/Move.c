#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Move instruction - MOVE class
 *
 * Transfer data from source to destination operand.
 * The source is unaffected, destination is overwritten.
 *
 * Variants: 6 (by data type)
 * Mnemonics: BI MOVE, BY MOVE, H MOVE, W MOVE, F MOVE, D MOVE
 * Operands: 2 (source, destination)
 *
 * Opcodes:
 *   0xFC0B (BI MOVE - bit)
 *   0x0019 (BY MOVE - byte)
 *   0xFC14 (H MOVE - halfword)
 *   0x001A (W MOVE - word)
 *   0x001B (F MOVE - float, 32-bit)
 *   0x002C (D MOVE - double, 64-bit)
 *
 * Operation: source → destination
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if source value is zero
 *   S = 1 if source value sign bit is set
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Move.cs
 */
void nd500_instr_Move(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] MOVE at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read source operand value (like C# ReadOperandValue) */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Write to destination operand (like C# WriteOperandValue) */
    nd500_write_operand_value(cpu, &fi->operands[1], value, fi->data_type);

    /* Update status flags: Z, S */
    nd500_set_flags_zs(cpu, value, fi->data_type);
}
