#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Add instruction - ARITHMETIC class
 *
 * Add Operand to Register: Rn + <operand> → Rn
 *
 * Variants: 5
 * Mnemonics: + (add)
 * Operands: 1 (<operand/r/t>)
 *
 * Opcodes:
 *   0xFC34 (BYn +) byte add
 *   0xFC38 (Hn +)  halfword add
 *   0x0054 (Wn +)  word add
 *   0x0058 (Fn +)  float add
 *   0x005C (Dn +)  double add
 *
 * Operation: Rn + <operand> → Rn
 *
 * Description:
 *   The operand is added to the contents of the specified register.
 *   The result is stored in the register.
 *
 *   Register selection is encoded in opcode bits 1-0:
 *   - 00 → register 1 (I1, A1)
 *   - 01 → register 2 (I2, A2)
 *   - 10 → register 3 (I3, A3)
 *   - 11 → register 4 (I4, A4)
 *
 *   For integer variants: Uses I1-I4 registers
 *   For float/double: Uses A1-A4 (float) or D1-D4 (A+E pairs, double)
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if sum is zero
 *   S = 1 if sign bit is set
 *   C = 1 if carry from most significant bit (integer only)
 *   O = 1 if overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Basic Arithmetic)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Add.cs
 */
void nd500_instr_Add(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] ADD at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check for float/double variants (not yet implemented) */
    if (fi->uses_float_registers) {
        printf("[STUB] ADD at PC=0x%08X: Float/double addition not yet implemented (opcode 0x%04X)\n",
               fi->address, fi->opcode);
        return;
    }

    /* Read register and operand (like C# ReadIntegerRegister + ReadOperandValue) */
    uint64_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t operand = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Perform addition */
    uint64_t result = reg_value + operand;

    /* Detect carry and overflow before masking (using helpers to avoid duplication) */
    bool carry = nd500_detect_carry_add(result, fi->data_type);
    bool overflow = nd500_detect_add_overflow(reg_value, operand, result, fi->data_type);

    /* Mask to data type (like C# MaskToDataType) */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to register */
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    /* Update status flags: Z, S, C, O */
    nd500_set_flags_zsco(cpu, masked_result, fi->data_type, carry, overflow);
}
