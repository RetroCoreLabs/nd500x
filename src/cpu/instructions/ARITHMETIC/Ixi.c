#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Helper function to check if multiplication will overflow
 * (like C# lines 155-168)
 */
static bool will_multiply_overflow(int64_t a, int64_t b, Nd500DataType dataType) {
    switch (dataType) {
        case ND500_DTYPE_BYTE:
            /* Check if multiplication would exceed signed byte range */
            if (b == 0) return false;
            return (a > INT8_MAX / b) || (a < INT8_MIN / b);

        case ND500_DTYPE_HALFWORD:
            /* Check if multiplication would exceed signed halfword range */
            if (b == 0) return false;
            return (a > INT16_MAX / b) || (a < INT16_MIN / b);

        case ND500_DTYPE_WORD:
            /* Check if multiplication would exceed signed word range */
            if (b == 0) return false;
            return (a > INT32_MAX / b) || (a < INT32_MIN / b);

        default:
            return false;
    }
}

/**
 * Ixi instruction - ARITHMETIC class
 *
 * Integer Exponentiation: <i> ** <j> → Rn
 *
 * Variants: 3 (by data type and register)
 * Mnemonics: BYn IXI, Hn IXI, Wn IXI (n=1..4)
 * Operands: 2 (<i/r/t>, <j/r/t>)
 *
 * Opcodes:
 *   0xFCC8-0xFCCB (BY1 IXI through BY4 IXI) - Byte integer exponentiation
 *   0xFCCC-0xFCCF (H1 IXI through H4 IXI) - Halfword integer exponentiation
 *   0xFCD0-0xFCD3 (W1 IXI through W4 IXI) - Word integer exponentiation
 *
 * Operation: <i> ** <j> → Rn (datatype dependent part)
 *
 * Description:
 *   The integer operand <i> is raised to the power of the integer
 *   operand <j> and the result is stored in the specified register.
 *   For byte and halfword types, the result is stored in the lower part
 *   of the register. Negative exponents with non-unit bases give zero.
 *   Negative exponents with zero base cause illegal operand trap.
 *
 * Flags: Z (zero), S (sign), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   O = 1 if overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Illegal operand value (IOV)
 *
 * Reference: ND-500 Reference Manual, Chapter 12.2
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Ixi.cs
 */
void nd500_instr_Ixi(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 49-53) */
    if (fi->operand_count != 2) {
        printf("[ERROR] IXI at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate integer registers only (like C# lines 55-59) */
    if (fi->uses_float_registers) {
        printf("[ERROR] IXI at PC=0x%08X: Requires integer register\n",
               fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands (like C# lines 62-63) */
    int64_t baseValue = (int64_t)nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    int64_t exponent = (int64_t)nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Sign extend based on data type (like C# lines 66-80) */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            baseValue = (int8_t)baseValue;
            exponent = (int8_t)exponent;
            break;
        case ND500_DTYPE_HALFWORD:
            baseValue = (int16_t)baseValue;
            exponent = (int16_t)exponent;
            break;
        case ND500_DTYPE_WORD:
            baseValue = (int32_t)baseValue;
            exponent = (int32_t)exponent;
            break;
        default:
            printf("[ERROR] IXI at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    int64_t result = 0;
    bool overflow = false;
    bool illegalOperand = false;

    /* Handle special cases (like C# lines 87-106) */
    if (exponent == 0) {
        /* Any number to the power of 0 is 1 (like C# line 89) */
        result = 1;
    }
    else if (exponent < 0) {
        /* Negative exponent handling (like C# lines 91-105) */
        if (baseValue == 0) {
            /* 0^(negative) is illegal (like C# lines 93-97) */
            illegalOperand = true;
            result = 0;
        }
        else if (baseValue == 1 || baseValue == -1) {
            /* 1^n = 1, (-1)^n depends on parity (like C# lines 98-101) */
            result = (baseValue == 1) ? 1 : ((exponent % 2 == 0) ? 1 : -1);
        }
        else {
            /* Negative exponent with non-unit base gives 0 (like C# lines 102-105) */
            result = 0;
        }
    }
    else {
        /* Positive exponent - calculate power (like C# lines 108-121) */
        result = 1;
        for (int64_t i = 0; i < exponent; i++) {
            /* Check for overflow before multiplication (like C# lines 114-118) */
            if (will_multiply_overflow(result, baseValue, fi->data_type)) {
                overflow = true;
                /* Continue calculation to get least significant part */
            }
            result *= baseValue;  /* (like C# line 119) */
        }
    }

    /* Mask result to data type size (like C# line 124) */
    uint32_t maskedResult = nd500_mask_to_datatype((uint32_t)result, fi->data_type);

    /* Write result to register (like C# line 125) */
    nd500_write_integer_register(cpu, fi->target_register, maskedResult);

    /* Update status flags (like C# lines 127-139) */
    /* Set Z flag */
    if (maskedResult == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    /* Set S flag based on sign bit (like C# lines 131-138) */
    bool signBit = false;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            signBit = ((maskedResult & 0x80) != 0);
            break;
        case ND500_DTYPE_HALFWORD:
            signBit = ((maskedResult & 0x8000) != 0);
            break;
        case ND500_DTYPE_WORD:
            signBit = ((maskedResult & 0x80000000) != 0);
            break;
    }

    if (signBit) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }

    /* Set O flag (like C# line 139) */
    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    /* Handle trap conditions (like C# lines 142-149) */
    if (illegalOperand) {
        printf("[TRAP] IXI at PC=0x%08X: Negative exponent with zero base\n", fi->address);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    if (overflow) {
        printf("[TRAP] IXI at PC=0x%08X: Integer overflow\n", fi->address);
        trap_invalid_operation(cpu, fi->address);
        return;
    }
}
