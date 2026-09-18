#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Stz instruction - MOVE class
 *
 * Store Zero - Write zero to destination operand.
 *
 * Variants: 6 (by data type)
 * Mnemonics: BI STZ, BY STZ, H STZ, W STZ, F STZ, D STZ
 * Operands: 1 (destination)
 *
 * Opcodes:
 *   0xFC85 (BI STZ - bit store zero)
 *   0x0048 (BY STZ - byte store zero)
 *   0x0049 (H STZ - halfword store zero)
 *   0x004A (W STZ - word store zero)
 *   0x004B (F STZ - float store zero)
 *   0x004C (D STZ - double store zero)
 *
 * Operation: 0 -> destination
 *
 * Description:
 *   The contents of the destination operand are replaced by zero.
 *   This is more efficient than MOVE 0, <dest> since no source
 *   operand needs to be fetched.
 *
 * Flags: Z (always set to 1), S (always set to 0)
 *   Z = 1 (result is always zero)
 *   S = 0 (zero is not negative)
 *
 * Traps: Addressing traps only
 *
 * Reference: ND-500 Reference Manual and docs/instructions/asm/ (authoritative).
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Stz.cs
 */
void nd500_instr_Stz(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] STZ at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Write zero to destination operand (like C# WriteOperandValue) */
    nd500_write_operand_value(cpu, &fi->operands[0], 0, fi->data_type);

    /* Set status flags: Z=1 (always zero), S=0 (zero is not negative) */
    nd500_set_flag(cpu, ND500_FLAG_Z);
    nd500_clear_flag(cpu, ND500_FLAG_S);
}
