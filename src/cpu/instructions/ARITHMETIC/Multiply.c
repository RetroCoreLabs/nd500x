#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Multiply instruction - ARITHMETIC class
 *
 * Multiply Register by Operand: Rn * <operand> → Rn
 *
 * Variants: 5
 * Mnemonics: * (multiply)
 * Operands: 1 (<operand/r/t>)
 *
 * Opcodes:
 *   0xFC44 (BYn *) byte multiply
 *   0xFC48 (Hn *)  halfword multiply
 *   0x006C (Wn *)  word multiply
 *   0x0070 (Fn *)  float multiply
 *   0x0074 (Dn *)  double multiply
 *
 * Operation: Rn * <operand> → Rn
 *
 * Description:
 *   The contents of the specified register are multiplied by the operand.
 *   The result is stored in the register. For integer types, only the lower
 *   part of the result is stored (upper part discarded). Overflow occurs if
 *   the result doesn't fit in the register.
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
 *   Z = 1 if product is zero
 *   S = 1 if sign bit is set
 *   C = 1 if carry from most significant bit (upper part non-zero)
 *   O = 1 if overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Basic Arithmetic)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Multiply.cs
 */
void nd500_instr_Multiply(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] MULTIPLY at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check for float/double variants (not yet implemented) */
    if (fi->uses_float_registers) {
        printf("[STUB] MULTIPLY at PC=0x%08X: Float/double multiplication not yet implemented (opcode 0x%04X)\n",
               fi->address, fi->opcode);
        return;
    }

    /* Read register and operand (like C# ReadIntegerRegister + ReadOperandValue) */
    uint64_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t operand = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Sign-extend both operands based on data type (signed multiplication) */
    int64_t signed_reg = 0;
    int64_t signed_operand = 0;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            signed_reg = (int8_t)(reg_value & 0xFF);
            signed_operand = (int8_t)(operand & 0xFF);
            break;
        case ND500_DTYPE_HALFWORD:
            signed_reg = (int16_t)(reg_value & 0xFFFF);
            signed_operand = (int16_t)(operand & 0xFFFF);
            break;
        case ND500_DTYPE_WORD:
            signed_reg = (int32_t)(reg_value & 0xFFFFFFFF);
            signed_operand = (int32_t)(operand & 0xFFFFFFFF);
            break;
        default:
            signed_reg = (int64_t)reg_value;
            signed_operand = (int64_t)operand;
            break;
    }

    /* Perform signed multiplication */
    int64_t product = signed_reg * signed_operand;

    /* Detect overflow and carry (like C# - masks match uint promotion to long) */
    bool overflow = false;
    bool carry = false;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            overflow = (product < INT8_MIN || product > INT8_MAX);
            carry = ((product & 0xFFFFFF00ULL) != 0);  /* Matches C# uint 0xFFFFFF00 */
            break;
        case ND500_DTYPE_HALFWORD:
            overflow = (product < INT16_MIN || product > INT16_MAX);
            carry = ((product & 0xFFFF0000ULL) != 0);  /* Matches C# uint 0xFFFF0000 */
            break;
        case ND500_DTYPE_WORD:
            overflow = (product < INT32_MIN || product > INT32_MAX);
            carry = ((product >> 32) != 0);  /* Matches C# arithmetic right shift */
            break;
        default:
            overflow = false;
            carry = false;
            break;
    }

    /* Mask to data type (lower part only, like C# MaskToDataType) */
    uint32_t masked_result = nd500_mask_to_datatype((uint64_t)product, fi->data_type);

    /* Write back to register */
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    /* Update status flags: Z, S, O (MUL clears carry per ND-500 Manual 11.7) */
    nd500_set_flags_zsco(cpu, masked_result, fi->data_type, false, overflow);
}
