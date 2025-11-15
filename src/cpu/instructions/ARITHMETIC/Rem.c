#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Rem instruction - ARITHMETIC class
 *
 * Remainder (Modulo) Operation: <dividend> % <divisor> → <result>
 *
 * Variants: 8
 * Mnemonics: rem
 * Operands: 3 (<dividend/r/t>, <divisor/r/t>, <result/w/t>)
 *
 * Opcodes:
 *   0xFE58-0xFE5F (rem) - 8 variants for different addressing modes
 *
 * Operation: <dividend> % <divisor> → <result>
 *
 * Description:
 *   Computes the integer remainder of dividing the dividend by the divisor,
 *   storing the result in the destination operand. This implements the modulo
 *   operation (dividend mod divisor), which returns the remainder after
 *   integer division.
 *
 *   The remainder has the same sign as the dividend and satisfies:
 *     dividend = (dividend / divisor) * divisor + remainder
 *
 *   Examples:
 *     17 REM 5 = 2 (because 17 = 3×5 + 2)
 *     -17 REM 5 = -2 (because -17 = -4×5 + (-2))
 *     17 REM -5 = 2 (because 17 = -3×(-5) + 2)
 *
 *   Data Types: F (float), D (double), R (register/integer)
 *
 * Flags: Z (zero), S (sign), V (overflow), C (undefined)
 *   Z = 1 if remainder is zero (dividend evenly divisible)
 *   S = 1 if remainder is negative (matches dividend sign)
 *   V = 1 if floating-point overflow
 *   C = undefined for REM
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Divide fault (DVF) if divisor is zero
 *   - Floating-point exceptions (for F/D types)
 *
 * Key Characteristics:
 *   - Three-operand form allows flexible operand combinations
 *   - Sign of remainder matches sign of dividend (not divisor)
 *   - Division by zero traps with divide fault
 *   - Quotient is discarded (use DIV for quotient only)
 *   - Use DIV4 when both quotient and remainder are needed
 *
 * Common Use Cases:
 *   - Modular arithmetic (hash functions, cyclic buffers)
 *   - Even/odd detection (n REM 2)
 *   - Digit extraction (number REM 10)
 *   - Array index wrapping (index REM array_size)
 *   - Time calculations (seconds REM 60, minutes REM 60)
 *
 * Performance Notes:
 *   - For power-of-2 divisors, use AND with mask instead (faster)
 *   - Always traps on zero divisor - check beforehand if needed
 *   - If both quotient and remainder needed, use DIV4
 *
 * Reference: ND-500 Reference Manual, §11.x (Remainder operation)
 *            /home/ronny/repos/nd500x/docs/instructions/asm/rem.md
 */
void nd500_instr_Rem(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (!nd500_validate_operand_count(cpu, fi, 3, INSTR_REM)) return;
    if (nd500_check_float_stub(fi, INSTR_REM)) return;

    // Read operands
    uint64_t dividend = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    uint64_t divisor = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    // Check for divide by zero
    if (divisor == 0) {
        cpu->ST1 |= ND500_FLAG_DZ;
        trap_divide_by_zero(cpu, fi->address);
        return;
    }

    // Clear divide-by-zero flag
    cpu->ST1 &= ~ND500_FLAG_DZ;

    // Sign-extend operands based on data type (signed arithmetic)
    int64_t signed_dividend = 0;
    int64_t signed_divisor = 0;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            signed_dividend = (int8_t)(dividend & 0xFF);
            signed_divisor = (int8_t)(divisor & 0xFF);
            break;
        case ND500_DTYPE_HALFWORD:
            signed_dividend = (int16_t)(dividend & 0xFFFF);
            signed_divisor = (int16_t)(divisor & 0xFFFF);
            break;
        case ND500_DTYPE_WORD:
            signed_dividend = (int32_t)(dividend & 0xFFFFFFFF);
            signed_divisor = (int32_t)(divisor & 0xFFFFFFFF);
            break;
        default:
            signed_dividend = (int64_t)dividend;
            signed_divisor = (int64_t)divisor;
            break;
    }

    // Compute remainder (modulo)
    // C % operator: remainder has sign of dividend (matches ND-500 behavior)
    int64_t remainder = signed_dividend % signed_divisor;

    // Mask result to data type
    uint32_t masked_result = nd500_mask_to_datatype((uint64_t)remainder, fi->data_type);

    // Write result to destination operand
    nd500_write_operand_value(cpu, &fi->operands[2], masked_result, fi->data_type);

    // Update status flags
    if (masked_result == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    // Set sign bit based on data type
    bool sign_bit = nd500_is_negative(masked_result, fi->data_type);
    if (sign_bit) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }

    // Overflow flag is not set for REM (only for floating-point variants)
    cpu->ST1 &= ~ND500_FLAG_O;
}
