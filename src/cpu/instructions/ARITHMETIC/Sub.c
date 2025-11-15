#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Sub instruction - ARITHMETIC class
 *
 * Subtract operand from register. Rn = Rn - operand
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn-, Hn-, Wn-, Fn-, Dn- (n=1..4)
 * Operands: 1 (value to subtract)
 *
 * Opcodes:
 *   0xFC3C-0xFC3F (BY1- through BY4-) - Byte subtract
 *   0xFC40-0xFC43 (H1- through H4-) - Halfword subtract
 *   0x0060-0x0063 (W1- through W4-) - Word subtract
 *   0x0064-0x0067 (F1- through F4-) - Float subtract (NOT IMPLEMENTED)
 *   0x0068-0x006B (D1- through D4-) - Double subtract (NOT IMPLEMENTED)
 *
 * Operation: Rn ← Rn - operand
 *
 * Flags: Z (zero), S (sign), C (carry/borrow), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if borrow occurred (minuend < subtrahend)
 *   O = 1 if signed overflow occurred
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Subtract.cs
 */
void nd500_instr_Sub(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] SUB at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check for float/double variants (not yet implemented) */
    if (fi->uses_float_registers) {
        printf("[STUB] SUB at PC=0x%08X: Float/double subtraction not yet implemented (opcode 0x%04X)\n",
               fi->address, fi->opcode);
        return;
    }

    /* Read register and operand (like C# ReadIntegerRegister + ReadOperandValue) */
    uint64_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t operand = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Perform subtraction */
    uint64_t result = reg_value - operand;

    /* Detect carry/borrow (when minuend < subtrahend) */
    bool carry = (reg_value < operand);
    bool overflow = nd500_detect_sub_overflow(reg_value, operand, result, fi->data_type);

    /* Mask to data type (like C# MaskToDataType) */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to register */
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    /* Update status flags: Z, S, C, O */
    nd500_set_flags_zsco(cpu, masked_result, fi->data_type, carry, overflow);
}
