#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Subtract instruction - ARITHMETIC class
 *
 * Subtract from Register: Rn - <operand> → Rn
 *
 * Variants: 5
 * Mnemonics: - (subtract)
 * Operands: 1 (<operand/r/t>)
 *
 * Opcodes:
 *   0xFC3C (BYn -) byte subtract
 *   0xFC40 (Hn -)  halfword subtract
 *   0x0060 (Wn -)  word subtract
 *   0x0064 (Fn -)  float subtract
 *   0x0068 (Dn -)  double subtract
 *
 * Operation: Rn - <operand> → Rn
 *
 * Description:
 *   The operand is subtracted from the contents of the specified register.
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
 *   Z = 1 if difference is zero
 *   S = 1 if sign bit is set
 *   C = 1 if borrow occurred (integer only)
 *   O = 1 if overflow (integer or float overflow)
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Basic Arithmetic)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Subtract.cs
 */
void nd500_instr_Subtract(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count and target register using helper functions
    if (!nd500_validate_operand_count(cpu, fi, 1, INSTR_SUBTRACT)) return;
    if (!nd500_validate_target_register(cpu, fi, INSTR_SUBTRACT)) return;
    if (nd500_check_float_stub(fi, INSTR_SUBTRACT)) return;

    // Integer subtraction
    uint32_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t operand = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    // Perform subtraction
    uint64_t result = reg_value - operand;

    // Detect borrow (carry) - borrow if reg_value < operand
    bool carry = (reg_value < operand);

    // Detect overflow
    bool overflow = nd500_detect_sub_overflow(reg_value, operand, result, fi->data_type);

    // Mask result to data type
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    // Write back to register
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    // Update status flags
    if (masked_result == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    if (carry) {
        cpu->ST1 |= ND500_FLAG_C;
    } else {
        cpu->ST1 &= ~ND500_FLAG_C;
    }

    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    // Set sign bit based on data type
    bool sign_bit = nd500_is_negative(masked_result, fi->data_type);
    if (sign_bit) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }
}
