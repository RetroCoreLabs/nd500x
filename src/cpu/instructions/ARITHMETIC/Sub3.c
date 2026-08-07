#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Sub3 instruction - ARITHMETIC class
 *
 * Extended Subtract (Three Operands): <a> - <b> → <c>
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn SUB3, Hn SUB3, Wn SUB3, Fn SUB3, Dn SUB3 (n=1..4)
 * Operands: 3 (<a/r/t>, <b/r/t>, <c/w/t>)
 *
 * Opcodes:
 *   0xFC54-0xFC57 (BY1 SUB3 through BY4 SUB3) - Byte extended subtract
 *   0xFC58-0xFC5B (H1 SUB3 through H4 SUB3) - Halfword extended subtract
 *   0x0084-0x0087 (W1 SUB3 through W4 SUB3) - Word extended subtract
 *   0x0088-0x008B (F1 SUB3 through F4 SUB3) - Float extended subtract
 *   0x008C-0x008F (D1 SUB3 through D4 SUB3) - Double extended subtract
 *
 * Operation: <a> - <b> → <c>
 *
 * Description:
 *   The <b> operand is subtracted from the <a> operand and the result
 *   is stored in the <c> operand location. This is a three-operand
 *   version that allows subtraction without affecting any register contents.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if NO borrow (minuend >= subtrahend)
 *   O = 1 if overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Extended Arithmetic)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Sub3.cs
 */
void nd500_instr_Sub3(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 53-57) */
    if (fi->operand_count != 3) {
        printf("[ERROR] SUB3 at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double types.
     * Microcode SUB3F @002241 / SUB3D: a - b -> <c>; ST,SAVF sets Z,S,FU,FO; C,O cleared (rule 4040). */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        double aValue = nd500_read_operand_as_ieee_float(cpu, &fi->operands[0], is_double);
        double bValue = nd500_read_operand_as_ieee_float(cpu, &fi->operands[1], is_double);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        double fresult = aValue - bValue;
        nd500_write_operand_from_ieee_float(cpu, &fi->operands[2], fresult, is_double);
        nd500_float_finish(cpu, fi->address, fresult, is_double);
        return;
    }

    uint64_t aValue, bValue, result;
    bool overflow = false;
    bool carry = false;

    /* Read operand a value (like C# line 60) */
    aValue = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Read operand b value (like C# line 63) */
    bValue = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Perform subtraction: a - b (like C# lines 65-102) */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            /* Signed byte subtraction (like C# lines 72-80) */
            int8_t aByte = (int8_t)(aValue & 0xFF);
            int8_t bByte = (int8_t)(bValue & 0xFF);
            int32_t diff = (int32_t)aByte - (int32_t)bByte;
            result = (uint64_t)(uint8_t)(diff & 0xFF);
            overflow = (diff < -128 || diff > 127);
            /* ND-500 carry: C=1 means NO borrow (unsigned a >= b)
             * Reference: ND-500 Manual Page 2040, SUBC formula Page 194 */
            carry = ((aValue & 0xFF) >= (bValue & 0xFF));
            break;
        }

        case ND500_DTYPE_HALFWORD: {
            /* Signed halfword subtraction (like C# lines 83-91) */
            int16_t aHalf = (int16_t)(aValue & 0xFFFF);
            int16_t bHalf = (int16_t)(bValue & 0xFFFF);
            int32_t diff = (int32_t)aHalf - (int32_t)bHalf;
            result = (uint64_t)(uint16_t)(diff & 0xFFFF);
            overflow = (diff < -32768 || diff > 32767);
            /* ND-500 carry: C=1 means NO borrow (unsigned a >= b) */
            carry = ((aValue & 0xFFFF) >= (bValue & 0xFFFF));
            break;
        }

        case ND500_DTYPE_WORD: {
            /* Signed word subtraction (like C# lines 94-102) */
            int32_t aWord = (int32_t)(aValue & 0xFFFFFFFF);
            int32_t bWord = (int32_t)(bValue & 0xFFFFFFFF);
            int64_t diff = (int64_t)aWord - (int64_t)bWord;
            result = (uint64_t)(uint32_t)(diff & 0xFFFFFFFF);
            overflow = (diff < INT32_MIN || diff > INT32_MAX);
            /* ND-500 carry: C=1 means NO borrow (unsigned a >= b) */
            carry = ((aValue & 0xFFFFFFFF) >= (bValue & 0xFFFFFFFF));
            break;
        }

        default:
            printf("[ERROR] SUB3 at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Write result to operand c location (like C# line 143) */
    nd500_write_operand_value(cpu, &fi->operands[2], result, fi->data_type);

    /* Update status flags based on result (like C# lines 145-149) */
    nd500_set_flags_zsco(cpu, result, fi->data_type, carry, overflow);

    /* Handle trap conditions (like C# lines 151-163) */
    if (overflow) {
        ND500X_TRAPLOG("[TRAP] SUB3 at PC=0x%08X: Integer overflow\n", fi->address);
        trap_integer_overflow(cpu, fi->address);
        return;
    }
}
