#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * Incr instruction - ARITHMETIC class
 *
 * Increment operand by one. operand = operand + 1
 *
 * Variants: 5 (by data type)
 * Mnemonics: BY INCR, H INCR, W INCR, F INCR, D INCR
 * Operands: 1 (value to increment, must be read-write)
 *
 * Opcodes:
 *   0xFC8A (BY INCR) - Byte increment
 *   0x004E (H INCR) - Halfword increment
 *   0x004F (W INCR) - Word increment
 *   0x0050 (F INCR) - Float increment
 *   0xFC8B (D INCR) - Double increment
 *
 * Operation: operand <- operand + 1
 *
 * Description:
 *   The operand is incremented by one. The Carry bit is set if a carry
 *   occurs from the sign bit position of the adder, otherwise it is reset.
 *   Carry will occur when and only when integer -1 is incremented.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if carry from most significant bit (only when -1 incremented)
 *   O = 1 if signed overflow occurred (max positive value incremented)
 *
 * Traps: Addressing traps, Integer overflow (O)
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Incr.cs
 */
void nd500_instr_Incr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] INCR at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double variants */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);

        /* Read operand value */
        double value = 0.0;
        if (is_double) {
            uint64_t op_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
            value = nd500_double_to_ieee754(op_bits);
        } else {
            uint32_t op_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
            value = (double)nd500_float_to_ieee754(op_bits);
        }

        /* Perform increment by 1.0 */
        double result = value + 1.0;

        /* Check for overflow/underflow */
        if (isinf(result) || isnan(result)) {
            trap_floating_overflow(cpu, fi->address);
        }

        /* Convert result back to ND-500 format and write back */
        uint64_t result_bits = 0;
        if (is_double) {
            result_bits = nd500_double_from_ieee754(result);
            nd500_write_operand_value(cpu, &fi->operands[0], result_bits, ND500_DTYPE_DOUBLEWORD);
        } else {
            result_bits = nd500_float_from_ieee754((float)result);
            nd500_write_operand_value(cpu, &fi->operands[0], (uint32_t)result_bits, ND500_DTYPE_WORD);
        }

        /* Update flags: Z (zero), S (sign) */
        if (is_double) {
            if (nd500_double_is_zero(result_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_Z);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_Z);
            }
            if (nd500_double_is_negative(result_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_S);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_S);
            }
        } else {
            uint32_t float_bits = (uint32_t)result_bits;
            if (nd500_float_is_zero(float_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_Z);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_Z);
            }
            if (nd500_float_is_negative(float_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_S);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_S);
            }
        }
        return;
    }

    /* Read operand value (like C# ReadOperandValue) */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Perform increment */
    uint64_t result = value + 1;

    /* Detect carry and overflow before masking */
    bool carry = false;
    bool overflow = false;
    if (fi->data_type == ND500_DTYPE_BYTE) {
        carry = ((uint8_t)value == 0xFF);      /* Carry when -1 incremented */
        overflow = ((int8_t)value == 0x7F);     /* Overflow when max positive incremented */
    } else if (fi->data_type == ND500_DTYPE_HALFWORD) {
        carry = ((uint16_t)value == 0xFFFF);
        overflow = ((int16_t)value == 0x7FFF);
    } else if (fi->data_type == ND500_DTYPE_WORD) {
        carry = ((uint32_t)value == 0xFFFFFFFF);
        overflow = ((int32_t)value == 0x7FFFFFFF);
    }

    /* Mask to data type (like C# MaskToDataType) */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write result back to operand (like C# WriteOperandValue) */
    nd500_write_operand_value(cpu, &fi->operands[0], masked_result, fi->data_type);

    /* Update status flags: Z, S, C, O */
    nd500_set_flags_zsco(cpu, masked_result, fi->data_type, carry, overflow);
}
