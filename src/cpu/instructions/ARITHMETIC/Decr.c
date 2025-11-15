#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Decr instruction - ARITHMETIC class
 *
 * Decrement operand by one. operand = operand - 1
 *
 * Variants: 5 (by data type)
 * Mnemonics: BY DECR, H DECR, W DECR, F DECR, D DECR
 * Operands: 1 (value to decrement, must be read-write)
 *
 * Opcodes:
 *   0xFC86 (BY DECR) - Byte decrement
 *   0xFC87 (H DECR) - Halfword decrement
 *   0x0051 (W DECR) - Word decrement
 *   0xFC88 (F DECR) - Float decrement (NOT IMPLEMENTED)
 *   0xFC89 (D DECR) - Double decrement (NOT IMPLEMENTED)
 *
 * Operation: operand ← operand - 1
 *
 * Description:
 *   The operand is decremented by one. The Carry bit is set if a borrow
 *   occurs (when decrementing 0), otherwise it is reset.
 *
 * Flags: Z (zero), S (sign), C (carry/borrow), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if borrow occurred (decrementing 0)
 *   O = 1 if signed overflow occurred (most negative value decremented)
 *
 * Traps: Addressing traps, Integer overflow (O)
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Decr.cs
 */
void nd500_instr_Decr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] DECR at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check for float/double variants (not yet implemented) */
    if (fi->uses_float_registers) {
        printf("[STUB] DECR at PC=0x%08X: Float/double decrement not yet implemented (opcode 0x%04X)\n",
               fi->address, fi->opcode);
        return;
    }

    /* Read operand value (like C# ReadOperandValue) */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Perform decrement */
    uint64_t result = value - 1;

    /* Detect carry/borrow and overflow before masking */
    bool carry = false;
    bool overflow = false;
    if (fi->data_type == ND500_DTYPE_BYTE) {
        carry = ((uint8_t)value == 0);              /* Borrow when decrementing 0 */
        overflow = ((int8_t)value == (int8_t)0x80); /* Overflow when most negative decremented */
    } else if (fi->data_type == ND500_DTYPE_HALFWORD) {
        carry = ((uint16_t)value == 0);
        overflow = ((int16_t)value == (int16_t)0x8000);
    } else if (fi->data_type == ND500_DTYPE_WORD) {
        carry = ((uint32_t)value == 0);
        overflow = ((int32_t)value == (int32_t)0x80000000);
    }

    /* Mask to data type (like C# MaskToDataType) */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write result back to operand (like C# WriteOperandValue) */
    nd500_write_operand_value(cpu, &fi->operands[0], masked_result, fi->data_type);

    /* Update status flags: Z, S, C, O */
    nd500_set_flags_zsco(cpu, masked_result, fi->data_type, carry, overflow);
}
