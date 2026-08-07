#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Div3 instruction - ARITHMETIC class
 *
 * Extended Divide (Three Operands): <a> / <b> → <c>
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn DIV3, Hn DIV3, Wn DIV3, Fn DIV3, Dn DIV3 (n=1..4)
 * Operands: 3 (<a/r/t>, <b/r/t>, <c/w/t>)
 *
 * Opcodes:
 *   0xFC74-0xFC77 (BY1 DIV3 through BY4 DIV3) - Byte extended divide
 *   0xFC78-0xFC7B (H1 DIV3 through H4 DIV3) - Halfword extended divide
 *   0x00B4-0x00B7 (W1 DIV3 through W4 DIV3) - Word extended divide
 *   0x00B8-0x00BB (F1 DIV3 through F4 DIV3) - Float extended divide
 *   0x00BC-0x00BF (D1 DIV3 through D4 DIV3) - Double extended divide
 *
 * Operation: <a> / <b> → <c>
 *
 * Description:
 *   The <a> operand is divided by the <b> operand and the quotient
 *   is stored in the <c> operand location. This is a three-operand
 *   version that allows division without affecting any register contents.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 0 (division doesn't set carry)
 *   O = 1 if overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Divide by zero (DZ)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Extended Arithmetic)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Div3.cs
 */
void nd500_instr_Div3(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 55-59) */
    if (fi->operand_count != 3) {
        printf("[ERROR] DIV3 at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double types.
     * Microcode DIV3F / DIV3D: a / b -> <c>; ST,SAVF sets Z,S,FU,FO; C,O cleared (rule 4040).
     * Divide-by-zero sets DZ and raises the divide-by-zero trap. */
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
        if (bValue == 0.0) {
            cpu->ST1 |= ND500_FLAG_DZ;
            trap_divide_by_zero(cpu, fi->address);
            return;
        }
        cpu->ST1 &= ~ND500_FLAG_DZ;
        double fresult = aValue / bValue;
        nd500_write_operand_from_ieee_float(cpu, &fi->operands[2], fresult, is_double);
        nd500_float_finish(cpu, fi->address, fresult, is_double);
        return;
    }

    uint64_t aValue, bValue, result;
    bool overflow = false;

    /* Read operand a value (dividend) (like C# line 62) */
    aValue = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Read operand b value (divisor) (like C# line 65) */
    bValue = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Check for divide by zero (like C# lines 67-73) */
    if (bValue == 0) {
        ND500X_TRAPLOG("[TRAP] DIV3 at PC=0x%08X: Divide by zero\n", fi->address);
        trap_divide_by_zero(cpu, fi->address);
        return;
    }

    /* Perform division: a / b (like C# lines 75-108) */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            /* Signed byte division (like C# lines 81-88) */
            int8_t aByte = (int8_t)(aValue & 0xFF);
            int8_t bByte = (int8_t)(bValue & 0xFF);
            int32_t quotient = (int32_t)aByte / (int32_t)bByte;
            result = (uint64_t)(uint8_t)(quotient & 0xFF);
            overflow = (quotient < -128 || quotient > 127);
            break;
        }

        case ND500_DTYPE_HALFWORD: {
            /* Signed halfword division (like C# lines 91-98) */
            int16_t aHalf = (int16_t)(aValue & 0xFFFF);
            int16_t bHalf = (int16_t)(bValue & 0xFFFF);
            int32_t quotient = (int32_t)aHalf / (int32_t)bHalf;
            result = (uint64_t)(uint16_t)(quotient & 0xFFFF);
            overflow = (quotient < -32768 || quotient > 32767);
            break;
        }

        case ND500_DTYPE_WORD: {
            /* Signed word division (like C# lines 101-108) */
            int32_t aWord = (int32_t)(aValue & 0xFFFFFFFF);
            int32_t bWord = (int32_t)(bValue & 0xFFFFFFFF);
            int64_t quotient = (int64_t)aWord / (int64_t)bWord;
            result = (uint64_t)(uint32_t)(quotient & 0xFFFFFFFF);
            overflow = (quotient < INT32_MIN || quotient > INT32_MAX);
            break;
        }

        default:
            printf("[ERROR] DIV3 at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Write quotient to operand c location (like C# line 159) */
    nd500_write_operand_value(cpu, &fi->operands[2], result, fi->data_type);

    /* Update status flags based on result (like C# lines 161-165) */
    /* Set Z and S flags */
    nd500_set_flags_zs(cpu, result, fi->data_type);

    /* Clear carry flag - division doesn't set carry (like C# line 164) */
    cpu->ST1 &= ~ND500_FLAG_C;

    /* Set or clear overflow flag */
    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    /* Handle trap conditions (like C# lines 167-179) */
    if (overflow) {
        ND500X_TRAPLOG("[TRAP] DIV3 at PC=0x%08X: Integer overflow\n", fi->address);
        trap_integer_overflow(cpu, fi->address);
        return;
    }
}
