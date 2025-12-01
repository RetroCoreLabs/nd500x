#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Abs instruction - ARITHMETIC class
 *
 * Absolute value operation: |Rn| → Rn
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn ABS, Hn ABS, Wn ABS, Fn ABS, Dn ABS (n=1..4)
 * Operands: 0 (register-only operation)
 *
 * Opcodes:
 *   0xFE10-0xFE13 (BY1 ABS through BY4 ABS) - Byte absolute value
 *   0xFE14-0xFE17 (H1 ABS through H4 ABS) - Halfword absolute value
 *   0x0098-0x009B (W1 ABS through W4 ABS) - Word absolute value
 *   0x009C-0x009F (F1 ABS through F4 ABS) - Float absolute value
 *   0x00A0-0x00A3 (D1 ABS through D4 ABS) - Double absolute value
 *
 * Operation: |Rn| → Rn
 *
 * Description:
 *   The absolute value of the contents of the specified register is
 *   computed and stored back in the register. For integer types, this
 *   is done by taking the two's complement if the value is negative.
 *   For floating point types, this is done by clearing the sign bit.
 *   Byte and halfword absolute value will clear the upper part of the register.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if result is zero
 *   S = 0 (always - absolute value is never negative)
 *   C and O are not affected
 *
 * Trap conditions: None
 *
 * Reference: ND-500 Reference Manual, Chapter 10.15
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Abs.cs
 */
void nd500_instr_Abs(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 0) {
        printf("[ERROR] ABS at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint64_t value, result;

    /* Read current register value (like C# lines 48-51) */
    if (fi->uses_float_registers) {
        if (fi->data_type == ND500_DTYPE_WORD) { /* Float (F) */
            value = nd500_read_float_register(cpu, fi->target_register);
        } else { /* Double (D) */
            value = nd500_read_double_register(cpu, fi->target_register);
        }
    } else {
        value = nd500_read_integer_register(cpu, fi->target_register);
    }

    result = value;

    /* Perform absolute value operation (like C# lines 55-88) */
    if (!fi->uses_float_registers) {
        /* Integer types: negate if negative (two's complement) (like C# lines 55-73) */
        bool isNegative = false;
        bool isMinNegative = false;

        /* Check if value is negative by examining sign bit (like C# lines 58-64) */
        /* Also check for "most negative" values that overflow when negated */
        switch (fi->data_type) {
            case ND500_DTYPE_BYTE:
                isNegative = ((value & 0x80) != 0);
                isMinNegative = ((value & 0xFF) == 0x80);
                break;
            case ND500_DTYPE_HALFWORD:
                isNegative = ((value & 0x8000) != 0);
                isMinNegative = ((value & 0xFFFF) == 0x8000);
                break;
            case ND500_DTYPE_WORD:
                isNegative = ((value & 0x80000000) != 0);
                isMinNegative = ((value & 0xFFFFFFFF) == 0x80000000);
                break;
            default:
                isNegative = false;
                isMinNegative = false;
                break;
        }

        /* If negative, negate using two's complement (like C# lines 66-71) */
        if (isNegative) {
            result = (~value + 1);
            /* Mask to data type and clear upper bits for BY/H (like C# line 70) */
            result = nd500_mask_to_datatype(result, fi->data_type);
        }

        /* Write back to register (like C# line 73) */
        nd500_write_integer_register(cpu, fi->target_register, (uint32_t)result);

        /* Update status flags for integer (handle overflow case) */
        cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O);

        if (result == 0) {
            cpu->ST1 |= ND500_FLAG_Z;
        }

        if (isMinNegative) {
            /* Most negative value overflows: result stays negative, set S and O */
            cpu->ST1 |= ND500_FLAG_S;
            cpu->ST1 |= ND500_FLAG_O;
        }

        return;
    } else {
        /* Floating point: clear sign bit (like C# lines 75-88) */
        if (fi->data_type == ND500_DTYPE_WORD) { /* Float (F) */
            result = value & 0x7FFFFFFF;  /* Clear sign bit (like C# line 80) */
            nd500_write_float_register(cpu, fi->target_register, (uint32_t)result);
        } else { /* Double (D) */
            result = value & 0x7FFFFFFFFFFFFFFFull;  /* Clear sign bit (like C# line 85) */
            nd500_write_double_register(cpu, fi->target_register, result);
        }
    }

    /* Update status flags (like C# lines 90-92) */
    /* Clear Z and S flags first */
    cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S);

    /* Set Z flag if result is zero */
    if (result == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    }

    /* S flag remains cleared (absolute value is never negative) */
    /* C and O flags are not affected */
}
