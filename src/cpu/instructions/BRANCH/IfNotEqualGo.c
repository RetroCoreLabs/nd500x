#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * IfNotEqualGo instruction - BRANCH class
 *
 * Conditional jump if not equal (Z flag clear).
 *
 * Variants: 2 (by displacement size)
 * Mnemonics: IF <> GO:B, IF <> GO:H
 * Operands: 1 (signed displacement)
 *
 * Opcodes:
 *   0x00C6 (IF <> GO:B) - Byte displacement
 *   0x00C7 (IF <> GO:H) - Halfword displacement
 *
 * Operation: if Z = 0 then PC ← PC + displacement
 *
 * Description:
 *   Conditional jump if the zero flag is not set (result was non-zero).
 *
 * Flags: Unaffected
 *
 * Traps: Addressing traps, Branch trap (BT)
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/BRANCH/IfNotEqualGo.cs
 */
void nd500_instr_IfNotEqualGo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] IF<>GO at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check NOT Z flag condition */
    if (!nd500_test_flag(cpu, ND500_FLAG_Z)) {
        /* Read displacement value (like C# ReadOperandValue) */
        uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

        /* Sign-extend based on data type (using helper to avoid duplication) */
        int64_t displacement = nd500_sign_extend_by_dtype(value, fi->data_type);

        /* Update PC (relative branch) */
        cpu->PC = (uint32_t)(fi->address + fi->total_len + displacement);
    }
}
