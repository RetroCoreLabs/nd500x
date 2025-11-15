#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

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
 *   0x0050 (F INCR) - Float increment (NOT IMPLEMENTED)
 *   0xFC8B (D INCR) - Double increment (NOT IMPLEMENTED)
 *
 * Operation: operand ← operand + 1
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

    /* Check for float/double variants (not yet implemented) */
    if (fi->uses_float_registers) {
        printf("[STUB] INCR at PC=0x%08X: Float/double increment not yet implemented (opcode 0x%04X)\n",
               fi->address, fi->opcode);
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
