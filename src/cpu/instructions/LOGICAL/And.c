#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * And instruction - LOGICAL class
 *
 * Bitwise AND of register with operand. Rn = Rn AND operand
 *
 * Variants: 4 (by data type and register)
 * Mnemonics: BIn AND, BYn AND, Hn AND, Wn AND (n=1..4)
 * Operands: 1 (value to AND)
 *
 * Opcodes:
 *   0xFDCC-0xFDCF (BI1 AND through BI4 AND) - Bit AND
 *   0xFC90-0xFC93 (BY1 AND through BY4 AND) - Byte AND
 *   0xFC94-0xFC97 (H1 AND through H4 AND) - Halfword AND
 *   0x00E4-0x00E7 (W1 AND through W4 AND) - Word AND
 *
 * Operation: Rn <- Rn AND operand
 *
 * Description:
 *   A bitwise AND is performed between the contents of the specified register
 *   and the operand. The result is stored in the register. For BI, BY, and H
 *   data types, the upper part of the register is zero-filled.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *
 * Traps: Addressing traps only
 *
 * Reference: ND-500 Reference Manual and docs/instructions/asm/ (authoritative).
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/LOGICAL/And.cs
 */
void nd500_instr_And(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] AND at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read register and operand (like C# ReadIntegerRegister + ReadOperandValue) */
    uint32_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t operand = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Perform AND operation */
    uint32_t result = (uint32_t)(reg_value & operand);

    /* Mask to data type (clears upper bits for BI, BY, H - like C# MaskToDataType) */
    result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to register */
    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Update status flags */
    nd500_set_flags_zs(cpu, result, fi->data_type);
}
