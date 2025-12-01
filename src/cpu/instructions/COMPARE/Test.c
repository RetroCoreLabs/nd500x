#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Test instruction - COMPARE class
 *
 * Test operand against zero (compare with implicit zero).
 *
 * Variants: 6 (by data type)
 * Mnemonics: BI TEST, BY TEST, H TEST, W TEST, F TEST, D TEST
 * Operands: 1 (operand to test)
 *
 * Opcodes:
 *   0x0041 (BI TEST) - Bit test against zero
 *   0x0042 (BY TEST) - Byte test against zero
 *   0x0043 (H TEST) - Halfword test against zero
 *   0x0044 (W TEST) - Word test against zero
 *   0x0045 (F TEST) - Float test against zero
 *   0x0046 (D TEST) - Double test against zero
 *
 * Operation: operand - 0 (result discarded, flags updated)
 *
 * Description:
 *   This instruction is similar to comparing two operands, except that
 *   the second operand is implicitly zero. The operand is tested against
 *   zero and flags are updated accordingly.
 *
 * Flags: Z (zero), S (sign), C (carry - always 1 for integers)
 *   Z = 1 if operand is zero
 *   S = 1 if operand sign bit is set (no overflow for test against zero)
 *   C = 1 always (for integer types)
 *
 * Trap conditions: Addressing traps only
 *
 * Reference: ND-500 Reference Manual, Chapter 10.11
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/COMPARE/Test.cs
 */
void nd500_instr_Test(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] TEST at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operand (like C# line 48) */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Test against zero (result = value - 0 = value) (like C# line 51) */
    /* Update Z and S flags */
    nd500_set_flags_zs(cpu, value, fi->data_type);

    /* For integer types, C is always 1 (like C# lines 54-55) */
    /* Check uses_float_registers to distinguish integer from float */
    if (!fi->uses_float_registers) {
        nd500_set_flag(cpu, ND500_FLAG_C);
    }
}
