#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Mul2 instruction - ARITHMETIC class
 *
 * Extended Multiply (Two Operands): <a> * Rn → <b>
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn MUL2, Hn MUL2, Wn MUL2, Fn MUL2, Dn MUL2 (n=1..4)
 * Operands: 2 (<a/r/t>, <b/w/t>)
 *
 * Opcodes:
 *   0xFC5C-0xFC5F (BY1 MUL2 through BY4 MUL2) - Byte extended multiply
 *   0xFC60-0xFC63 (H1 MUL2 through H4 MUL2) - Halfword extended multiply
 *   0x0090-0x0093 (W1 MUL2 through W4 MUL2) - Word extended multiply
 *   0x0094-0x0097 (F1 MUL2 through F4 MUL2) - Float extended multiply
 *   0x0098-0x009B (D1 MUL2 through D4 MUL2) - Double extended multiply
 *
 * Operation: <a> * Rn → <b>
 *
 * Description:
 *   The <a> operand is multiplied by the contents of the specified register
 *   and the result is stored in the <b> operand location. This is an extended
 *   version of the basic MULTIPLY instruction.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 0 (multiplication doesn't set carry)
 *   O = 1 if overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Extended Arithmetic)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Mul2.cs
 */
void nd500_instr_Mul2(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 54-58) */
    if (fi->operand_count != 2) {
        printf("[ERROR] MUL2 at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double types - DEFERRED */
    if (fi->uses_float_registers) {
        printf("[DEFERRED] MUL2 at PC=0x%08X: Float/Double operations not yet implemented\n",
               fi->address);
        /* For now, just skip - will implement when float conversion helpers are ready */
        return;
    }

    uint64_t aValue, registerValue, result;
    bool overflow = false;

    /* Read first multiplicand from destination operand (operands[0]) */
    /* Note: Assembly format is "MUL2 <b>, <a>" where operands[0]=dest, operands[1]=src */
    aValue = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Read second multiplicand from source operand (operands[1]) */
    registerValue = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Perform multiplication: a * Rn (like C# lines 66-99) */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            /* Signed byte multiplication (like C# lines 72-79) */
            int8_t aByte = (int8_t)(aValue & 0xFF);
            int8_t regByte = (int8_t)(registerValue & 0xFF);
            int32_t product = (int32_t)aByte * (int32_t)regByte;
            result = (uint64_t)(uint8_t)(product & 0xFF);
            overflow = (product < -128 || product > 127);
            break;
        }

        case ND500_DTYPE_HALFWORD: {
            /* Signed halfword multiplication (like C# lines 82-89) */
            int16_t aHalf = (int16_t)(aValue & 0xFFFF);
            int16_t regHalf = (int16_t)(registerValue & 0xFFFF);
            int32_t product = (int32_t)aHalf * (int32_t)regHalf;
            result = (uint64_t)(uint16_t)(product & 0xFFFF);
            overflow = (product < -32768 || product > 32767);
            break;
        }

        case ND500_DTYPE_WORD: {
            /* Signed word multiplication (like C# lines 92-99) */
            int32_t aWord = (int32_t)(aValue & 0xFFFFFFFF);
            int32_t regWord = (int32_t)(registerValue & 0xFFFFFFFF);
            int64_t product = (int64_t)aWord * (int64_t)regWord;
            result = (uint64_t)(uint32_t)(product & 0xFFFFFFFF);
            overflow = (product < INT32_MIN || product > INT32_MAX);
            break;
        }

        default:
            printf("[ERROR] MUL2 at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Write result to operand b location (like C# line 150) */
    /* Write result to destination operand (operands[0]) */
    nd500_write_operand_value(cpu, &fi->operands[0], result, fi->data_type);

    /* Update status flags based on result (like C# lines 152-156) */
    /* Set Z and S flags */
    nd500_set_flags_zs(cpu, result, fi->data_type);

    /* Clear carry flag - multiplication doesn't set carry (like C# line 155) */
    cpu->ST1 &= ~ND500_FLAG_C;

    /* Set or clear overflow flag */
    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    /* Handle trap conditions (like C# lines 158-170) */
    if (overflow) {
        printf("[TRAP] MUL2 at PC=0x%08X: Integer overflow\n", fi->address);
        trap_invalid_operation(cpu, fi->address);
        return;
    }
}
