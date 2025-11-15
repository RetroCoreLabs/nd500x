#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Go instruction - BRANCH class
 *
 * Unconditional PC-relative branch. Adds signed displacement to PC.
 *
 * Variants: 3 (by operand size)
 * Mnemonics: go go go
 * Operands: 1 (displacement)
 *
 * Opcodes:
 *   0x00C0 (GO:B) - Byte displacement (-128 to +127)
 *   0x00C1 (GO:H) - Halfword displacement (-32768 to +32767)
 *   0x00C2 (GO:W) - Word displacement (full 32-bit)
 *
 * Operation:
 *   PC ← PC + displacement (sign-extended)
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/BRANCH/Go.cs
 */
void nd500_instr_Go(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] GO at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read displacement value (like C# ReadOperandValue) */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Sign-extend based on data type (using helper to avoid duplication) */
    int64_t displacement = nd500_sign_extend_by_dtype(value, fi->data_type);

    /* Update PC (relative to current PC) */
    /* Note: PC has already been advanced past this instruction by cpu_step(),
     * so we're branching relative to the address AFTER this instruction */
    cpu->PC = (uint32_t)((int64_t)cpu->PC + displacement);

    /* Branch trap (BT) would be checked by trap system if enabled */
    /* Note: This is an ignorable trap that doesn't stop execution */
}
