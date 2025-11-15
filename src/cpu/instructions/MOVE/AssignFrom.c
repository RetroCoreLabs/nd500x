#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * AssignFrom instruction - MOVE class
 *
 * Store register to destination operand. Rn =: <dest>
 *
 * Variants: 6 (by data type and register)
 * Mnemonics: BIn=:, BYn=:, Hn=:, Wn=:, Fn=:, Dn=: (n=1..4)
 * Operands: 1 (destination)
 *
 * Opcodes:
 *   0xFC0C-0xFC0F (BI1=: through BI4=:) - Store bit
 *   0x001C-0x001F (BY1=: through BY4=:) - Store byte
 *   0xFC10-0xFC13 (H1=: through H4=:) - Store halfword
 *   0x0020-0x0023 (W1=: through W4=:) - Store word
 *   0x0024-0x0027 (F1=: through F4=:) - Store float (NOT IMPLEMENTED)
 *   0x0028-0x002B (D1=: through D4=:) - Store double (NOT IMPLEMENTED)
 *
 * Operation: Rn → <dest>
 *
 * Description:
 *   The datatype-dependent part of the register is stored in the memory
 *   location or register specified in the operand. The source register
 *   is unaffected. If destination is a register, the upper part is zero-filled
 *   for BI, BY, or H data types.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if stored value is zero
 *   S = 1 if stored value sign bit is set
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/AssignFrom.cs
 */
void nd500_instr_AssignFrom(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] ASSIGNFROM at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check for float/double variants (not yet implemented) */
    if (fi->uses_float_registers) {
        printf("[STUB] ASSIGNFROM at PC=0x%08X: Float/double not yet implemented (opcode 0x%04X)\n",
               fi->address, fi->opcode);
        return;
    }

    /* Read value from source register (like C# ReadIntegerRegister) */
    uint64_t reg_value = nd500_read_integer_register(cpu, fi->target_register);

    /* Mask to data type (like C# MaskToDataType) */
    uint64_t value = nd500_mask_to_datatype(reg_value, fi->data_type);

    /* Write to destination operand (like C# WriteOperandValue) */
    nd500_write_operand_value(cpu, &fi->operands[0], value, fi->data_type);

    /* Update status flags: Z and S based on stored value (like C# SetStatusZS) */
    nd500_set_flags_zs(cpu, value, fi->data_type);
}
