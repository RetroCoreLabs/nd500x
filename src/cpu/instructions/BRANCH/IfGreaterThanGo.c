#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * IfGreaterThanGo instruction - BRANCH class
 *
 * Conditional jump if greater than (signed, S=0 and Z=0).
 *
 * Variants: 2 (by displacement size)
 * Mnemonics: IF > GO:B, IF > GO:H
 * Operands: 1 (signed displacement)
 *
 * Opcodes:
 *   0x00C8 (IF > GO:B) - Byte displacement
 *   0x00C9 (IF > GO:H) - Halfword displacement
 *
 * Operation: if S = 0 and Z = 0 then PC <- PC + displacement
 *
 * Description:
 *   Conditional jump if the result was greater than zero (signed comparison).
 *
 * Flags: Unaffected
 *
 * Traps: Addressing traps, Branch trap (BT)
 *
 * Reference: ND-500 Reference Manual and docs/instructions/asm/ (authoritative).
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/BRANCH/IfGreaterThanGo.cs
 */
void nd500_instr_IfGreaterThanGo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] IF>GO at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check S = 0 and Z = 0 condition */
    if (!nd500_test_flag(cpu, ND500_FLAG_S) && !nd500_test_flag(cpu, ND500_FLAG_Z)) {
        /* Read displacement value (like C# ReadOperandValue) */
        uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        /* Sign-extend based on data type (using helper to avoid duplication) */
        int64_t displacement = nd500_sign_extend_by_dtype(value, fi->data_type);

        /* Update PC (relative branch from instruction start) */
        cpu->PC = (uint32_t)(fi->address + displacement);
    }
}
