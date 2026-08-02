#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Div2 instruction - ARITHMETIC class
 *
 * Extended Divide (Two Operands): <a> / Rn → <b>
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn DIV2, Hn DIV2, Wn DIV2, Fn DIV2, Dn DIV2 (n=1..4)
 * Operands: 2 (<a/r/t>, <b/w/t>)
 *
 * Opcodes:
 *   0xFC6C-0xFC6F (BY1 DIV2 through BY4 DIV2) - Byte extended divide
 *   0xFC70-0xFC73 (H1 DIV2 through H4 DIV2) - Halfword extended divide
 *   0x00A8-0x00AB (W1 DIV2 through W4 DIV2) - Word extended divide
 *   0x00AC-0x00AF (F1 DIV2 through F4 DIV2) - Float extended divide
 *   0x00B0-0x00B3 (D1 DIV2 through D4 DIV2) - Double extended divide
 *
 * Operation: <a> / Rn → <b>
 *
 * Description:
 *   The <a> operand is divided by the contents of the specified register
 *   and the quotient is stored in the <b> operand location. This is an extended
 *   version of the basic DIVIDE instruction.
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
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Div2.cs
 */
void nd500_instr_Div2(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 55-59) */
    if (fi->operand_count != 2) {
        printf("[ERROR] DIV2 at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double types.
     * Microcode DIV2F / DIV2D: a / b -> <a>; ST,SAVF sets Z,S,FU,FO; C,O cleared (rule 4040).
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
        nd500_write_operand_from_ieee_float(cpu, &fi->operands[0], fresult, is_double);
        nd500_float_finish(cpu, fi->address, fresult, is_double);
        return;
    }

    uint64_t aValue, registerValue, result;
    bool overflow = false;

    /* Read dividend from destination operand (operands[0]) */
    /* Note: Assembly format is "DIV2 <b>, <a>" where operands[0]=dest/dividend, operands[1]=divisor */
    aValue = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Read divisor from source operand (operands[1]) */
    registerValue = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Check for divide by zero (like C# lines 67-73) */
    if (registerValue == 0) {
        printf("[TRAP] DIV2 at PC=0x%08X: Divide by zero\n", fi->address);
        trap_divide_by_zero(cpu, fi->address);
        return;
    }

    /* Perform division: a / Rn (like C# lines 75-108) */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            /* Signed byte division (like C# lines 81-88) */
            int8_t aByte = (int8_t)(aValue & 0xFF);
            int8_t regByte = (int8_t)(registerValue & 0xFF);
            int32_t quotient = (int32_t)aByte / (int32_t)regByte;
            result = (uint64_t)(uint8_t)(quotient & 0xFF);
            overflow = (quotient < -128 || quotient > 127);
            break;
        }

        case ND500_DTYPE_HALFWORD: {
            /* Signed halfword division (like C# lines 91-98) */
            int16_t aHalf = (int16_t)(aValue & 0xFFFF);
            int16_t regHalf = (int16_t)(registerValue & 0xFFFF);
            int32_t quotient = (int32_t)aHalf / (int32_t)regHalf;
            result = (uint64_t)(uint16_t)(quotient & 0xFFFF);
            overflow = (quotient < -32768 || quotient > 32767);
            break;
        }

        case ND500_DTYPE_WORD: {
            /* Signed word division (like C# lines 101-108) */
            int32_t aWord = (int32_t)(aValue & 0xFFFFFFFF);
            int32_t regWord = (int32_t)(registerValue & 0xFFFFFFFF);
            int64_t quotient = (int64_t)aWord / (int64_t)regWord;
            result = (uint64_t)(uint32_t)(quotient & 0xFFFFFFFF);
            overflow = (quotient < INT32_MIN || quotient > INT32_MAX);
            break;
        }

        default:
            printf("[ERROR] DIV2 at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Write quotient to destination operand (operands[0]) */
    nd500_write_operand_value(cpu, &fi->operands[0], result, fi->data_type);

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
        printf("[TRAP] DIV2 at PC=0x%08X: Integer overflow\n", fi->address);
        trap_invalid_operation(cpu, fi->address);
        return;
    }
}
