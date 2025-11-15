#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Div4 instruction - ARITHMETIC class
 *
 * Divide with Remainder to Register (Modulo): <a> / <b> → <c>, remainder → Rn
 *
 * Variants: 3 (by data type and register)
 * Mnemonics: BYn DIV4, Hn DIV4, Wn DIV4 (n=1..4)
 * Operands: 3 (<a/r/t>, <b/r/t>, <c/w/t>)
 *
 * Opcodes:
 *   0xFC2C-0xFC2F (BY1 DIV4 through BY4 DIV4) - Byte divide with remainder
 *   0xFC30-0xFC33 (H1 DIV4 through H4 DIV4) - Halfword divide with remainder
 *   0xFC7C-0xFC7F (W1 DIV4 through W4 DIV4) - Word divide with remainder
 *
 * Operation: <a> / <b> → <c>, remainder → Rn
 *
 * Description:
 *   The <a> operand is divided by the <b> operand and the quotient is
 *   stored in the <c> operand. The remainder is stored in the specified
 *   register. The register content complies with ADA and SIMULA remainder
 *   rules. Separate testing must be done to obtain status. The operands are
 *   assumed to have the same data type.
 *
 * Flags: Z (zero), S (sign), O (overflow), DZ (divide by zero)
 *   Z = 1 if quotient is zero
 *   S = 1 if quotient sign bit is set
 *   O = 1 if overflow (most negative value / -1)
 *   DZ = 1 if divisor is zero
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Divide by zero (DZ)
 *
 * Reference: ND-500 Reference Manual, Chapter 11.14
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Div4.cs
 */
void nd500_instr_Div4(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 52-56) */
    if (fi->operand_count != 3) {
        printf("[ERROR] DIV4 at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint64_t dividend, divisor;
    int64_t quotient = 0, remainder = 0;
    bool overflow = false;

    /* Read operand a value (dividend) (like C# line 59) */
    dividend = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Read operand b value (divisor) (like C# line 60) */
    divisor = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Check for divide by zero (like C# lines 63-68) */
    if (divisor == 0) {
        printf("[TRAP] DIV4 at PC=0x%08X: Divide by zero\n", fi->address);
        trap_divide_by_zero(cpu, fi->address);
        return;
    }

    /* Perform signed division with remainder (like C# lines 72-133) */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            /* Signed byte division (like C# lines 78-94) */
            int8_t a = (int8_t)(dividend & 0xFF);
            int8_t b = (int8_t)(divisor & 0xFF);

            /* Check for overflow (most negative / -1) (like C# lines 83-88) */
            if (a == INT8_MIN && b == -1) {
                overflow = true;
                quotient = a;  /* Result undefined, keep dividend */
                remainder = 0;
            } else {
                quotient = a / b;         /* (like C# line 91) */
                remainder = a % b;        /* (like C# line 92) */
            }
            break;
        }

        case ND500_DTYPE_HALFWORD: {
            /* Signed halfword division (like C# lines 96-111) */
            int16_t a = (int16_t)(dividend & 0xFFFF);
            int16_t b = (int16_t)(divisor & 0xFFFF);

            /* Check for overflow (like C# lines 100-105) */
            if (a == INT16_MIN && b == -1) {
                overflow = true;
                quotient = a;
                remainder = 0;
            } else {
                quotient = a / b;         /* (like C# line 108) */
                remainder = a % b;        /* (like C# line 109) */
            }
            break;
        }

        case ND500_DTYPE_WORD: {
            /* Signed word division (like C# lines 113-128) */
            int32_t a = (int32_t)(dividend & 0xFFFFFFFF);
            int32_t b = (int32_t)(divisor & 0xFFFFFFFF);

            /* Check for overflow (like C# lines 117-122) */
            if (a == INT32_MIN && b == -1) {
                overflow = true;
                quotient = a;
                remainder = 0;
            } else {
                quotient = a / b;         /* (like C# line 125) */
                remainder = a % b;        /* (like C# line 126) */
            }
            break;
        }

        default:
            printf("[ERROR] DIV4 at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Write quotient to operand c location (like C# line 136) */
    nd500_write_operand_value(cpu, &fi->operands[2], (uint64_t)quotient, fi->data_type);

    /* Write remainder to register (like C# line 139) */
    nd500_write_integer_register(cpu, fi->target_register, (uint32_t)remainder);

    /* Update status flags based on quotient (like C# lines 142-153) */
    /* Set Z flag based on quotient (like C# line 142) */
    if (quotient == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    /* Set S flag based on quotient sign bit (like C# lines 146-153) */
    bool signBit = false;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            signBit = ((quotient & 0x80) != 0);      /* (like C# line 149) */
            break;
        case ND500_DTYPE_HALFWORD:
            signBit = ((quotient & 0x8000) != 0);    /* (like C# line 150) */
            break;
        case ND500_DTYPE_WORD:
            signBit = ((quotient & 0x80000000) != 0); /* (like C# line 151) */
            break;
    }

    if (signBit) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }

    /* Set O flag based on overflow (like C# line 143) */
    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    /* Handle trap on overflow */
    if (overflow) {
        printf("[TRAP] DIV4 at PC=0x%08X: Integer overflow\n", fi->address);
        trap_invalid_operation(cpu, fi->address);
        return;
    }
}
