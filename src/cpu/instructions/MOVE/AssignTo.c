#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * AssignTo instruction - MOVE class
 *
 * Load register from source operand. Rn := <source>
 *
 * Variants: 6 (by data type and register)
 * Mnemonics: BIn:=, BYn:=, Hn:=, Wn:=, Fn:=, Dn:= (n=1..4)
 * Operands: 1 (source)
 *
 * Opcodes:
 *   0xFC04-0xFC07 (BI1:= through BI4:=) - Load bit
 *   0x0004-0x0007 (BY1:= through BY4:=) - Load byte
 *   0x0008-0x000B (H1:= through H4:=) - Load halfword
 *   0x000C-0x000F (W1:= through W4:=) - Load word
 *   0x0010-0x0013 (F1:= through F4:=) - Load float (NOT IMPLEMENTED)
 *   0x0014-0x0017 (D1:= through D4:=) - Load double (NOT IMPLEMENTED)
 *
 * Operation: <source> → Rn
 *
 * Description:
 *   The value of the operand (<source>) is loaded into the register
 *   specified in the instruction code. The value is right-justified in
 *   the register. With BI, BY, or H data types, the upper part of the
 *   register is zero-filled.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if loaded value is zero
 *   S = 1 if loaded value sign bit is set
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/AssignTo.cs
 */
void nd500_instr_AssignTo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] ASSIGNTO at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check for float/double variants (not yet implemented) */
    if (fi->uses_float_registers) {
        printf("[STUB] ASSIGNTO at PC=0x%08X: Float/double not yet implemented (opcode 0x%04X)\n",
               fi->address, fi->opcode);
        return;
    }

    /* Read source operand value (like C# ReadOperandValue) */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Mask to data type and write to register (like C# MaskToDataType + WriteIntegerRegister) */
    /* Zero-extends for sub-word types (BI, BY, H) */
    uint32_t masked_value = nd500_mask_to_datatype(value, fi->data_type);
    nd500_write_integer_register(cpu, fi->target_register, masked_value);

    /* Update status flags: Z and S based on loaded value (like C# SetStatusZS) */
    nd500_set_flags_zs(cpu, value, fi->data_type);
}
