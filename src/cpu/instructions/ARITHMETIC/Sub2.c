#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Sub2 instruction - ARITHMETIC class
 *
 * Extended Subtract (Two Operands): <a> - Rn → <b>
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn SUB2, Hn SUB2, Wn SUB2, Fn SUB2, Dn SUB2 (n=1..4)
 * Operands: 2 (<a/r/t>, <b/w/t>)
 *
 * Opcodes:
 *   0xFC4C-0xFC4F (BY1 SUB2 through BY4 SUB2) - Byte extended subtract
 *   0xFC50-0xFC53 (H1 SUB2 through H4 SUB2) - Halfword extended subtract
 *   0x0078-0x007B (W1 SUB2 through W4 SUB2) - Word extended subtract
 *   0x007C-0x007F (F1 SUB2 through F4 SUB2) - Float extended subtract
 *   0x0080-0x0083 (D1 SUB2 through D4 SUB2) - Double extended subtract
 *
 * Operation: <a> - Rn → <b>
 *
 * Description:
 *   The contents of the specified register (Rn) is subtracted from the
 *   <a> operand and the result is stored in the <b> operand location.
 *   This is an extended version of the basic SUBTRACT instruction.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if carry from most significant bit (integer only)
 *   O = 1 if overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Extended Arithmetic)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Sub2.cs
 */
void nd500_instr_Sub2(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 53-57) */
    if (fi->operand_count != 2) {
        printf("[ERROR] SUB2 at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double types - DEFERRED */
    if (fi->uses_float_registers) {
        printf("[DEFERRED] SUB2 at PC=0x%08X: Float/Double operations not yet implemented\n",
               fi->address);
        /* For now, just skip - will implement when float conversion helpers are ready */
        return;
    }

    uint64_t aValue, registerValue, result;
    bool overflow = false;
    bool carry = false;

    /* Read minuend from destination operand (operands[0]) */
    /* Note: Assembly format is "SUB2 <b>, <a>" where operands[0]=dest/minuend, operands[1]=subtrahend */
    aValue = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Read subtrahend from source operand (operands[1]) */
    registerValue = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Perform subtraction: a - Rn (like C# lines 65-102) */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            /* Signed byte subtraction (like C# lines 72-80) */
            int8_t aByte = (int8_t)(aValue & 0xFF);
            int8_t regByte = (int8_t)(registerValue & 0xFF);
            int32_t diff = (int32_t)aByte - (int32_t)regByte;
            result = (uint64_t)(uint8_t)(diff & 0xFF);
            overflow = (diff < -128 || diff > 127);
            carry = ((diff & 0x100) != 0);
            break;
        }

        case ND500_DTYPE_HALFWORD: {
            /* Signed halfword subtraction (like C# lines 83-91) */
            int16_t aHalf = (int16_t)(aValue & 0xFFFF);
            int16_t regHalf = (int16_t)(registerValue & 0xFFFF);
            int32_t diff = (int32_t)aHalf - (int32_t)regHalf;
            result = (uint64_t)(uint16_t)(diff & 0xFFFF);
            overflow = (diff < -32768 || diff > 32767);
            carry = ((diff & 0x10000) != 0);
            break;
        }

        case ND500_DTYPE_WORD: {
            /* Signed word subtraction (like C# lines 94-102) */
            int32_t aWord = (int32_t)(aValue & 0xFFFFFFFF);
            int32_t regWord = (int32_t)(registerValue & 0xFFFFFFFF);
            int64_t diff = (int64_t)aWord - (int64_t)regWord;
            result = (uint64_t)(uint32_t)(diff & 0xFFFFFFFF);
            overflow = (diff < INT32_MIN || diff > INT32_MAX);
            carry = ((diff & 0x100000000LL) != 0);
            break;
        }

        default:
            printf("[ERROR] SUB2 at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Write result to destination operand (operands[0]) */
    nd500_write_operand_value(cpu, &fi->operands[0], result, fi->data_type);

    /* Update status flags based on result (like C# lines 145-149) */
    nd500_set_flags_zsco(cpu, result, fi->data_type, carry, overflow);

    /* Handle trap conditions (like C# lines 151-163) */
    if (overflow) {
        printf("[TRAP] SUB2 at PC=0x%08X: Integer overflow\n", fi->address);
        trap_invalid_operation(cpu, fi->address);
        return;
    }
}
