#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Add2 instruction - ARITHMETIC class
 *
 * Extended Add (Two Operands): <a> + Rn → <b>
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn ADD2, Hn ADD2, Wn ADD2, Fn ADD2, Dn ADD2 (n=1..4)
 * Operands: 2 (<a/r/t>, <b/w/t>)
 *
 * Opcodes:
 *   0xFC3C-0xFC3F (BY1 ADD2 through BY4 ADD2) - Byte extended add
 *   0xFC40-0xFC43 (H1 ADD2 through H4 ADD2) - Halfword extended add
 *   0x0060-0x0063 (W1 ADD2 through W4 ADD2) - Word extended add
 *   0x0064-0x0067 (F1 ADD2 through F4 ADD2) - Float extended add
 *   0x0068-0x006B (D1 ADD2 through D4 ADD2) - Double extended add
 *
 * Operation: <a> + Rn → <b>
 *
 * Description:
 *   The <a> operand is added to the contents of the specified register.
 *   The result is stored in the <b> operand location. This is an extended
 *   version of the basic ADD instruction that allows storing the result
 *   to a memory location rather than back to the register.
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
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Add2.cs
 */
void nd500_instr_Add2(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 54-58) */
    if (fi->operand_count != 2) {
        printf("[ERROR] ADD2 at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double types - DEFERRED */
    if (fi->uses_float_registers) {
        printf("[DEFERRED] ADD2 at PC=0x%08X: Float/Double operations not yet implemented\n",
               fi->address);
        /* For now, just skip - will implement when float conversion helpers are ready */
        return;
    }

    uint64_t aValue, registerValue, result;
    bool overflow = false;
    bool carry = false;

    /* Read operand a (source) value - operands[1] is Source per metadata */
    /* Note: Assembly format is "ADD2 <b>, <a>" where operands[0]=dest, operands[1]=src */
    aValue = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Read Rn from destination operand (operands[0]) - Rn IS the destination register */
    /* Operation is: <a> + <b> → <b> where <b> is the destination register */
    registerValue = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Perform addition: a + Rn (like C# lines 66-103) */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            /* Signed byte addition (like C# lines 73-81) */
            int8_t aByte = (int8_t)(aValue & 0xFF);
            int8_t regByte = (int8_t)(registerValue & 0xFF);
            int32_t sum = (int32_t)aByte + (int32_t)regByte;
            result = (uint64_t)(uint8_t)(sum & 0xFF);
            overflow = (sum < -128 || sum > 127);
            carry = ((sum & 0x100) != 0);
            break;
        }

        case ND500_DTYPE_HALFWORD: {
            /* Signed halfword addition (like C# lines 84-92) */
            int16_t aHalf = (int16_t)(aValue & 0xFFFF);
            int16_t regHalf = (int16_t)(registerValue & 0xFFFF);
            int32_t sum = (int32_t)aHalf + (int32_t)regHalf;
            result = (uint64_t)(uint16_t)(sum & 0xFFFF);
            overflow = (sum < -32768 || sum > 32767);
            carry = ((sum & 0x10000) != 0);
            break;
        }

        case ND500_DTYPE_WORD: {
            /* Signed word addition (like C# lines 95-103) */
            int32_t aWord = (int32_t)(aValue & 0xFFFFFFFF);
            int32_t regWord = (int32_t)(registerValue & 0xFFFFFFFF);
            int64_t sum = (int64_t)aWord + (int64_t)regWord;
            result = (uint64_t)(uint32_t)(sum & 0xFFFFFFFF);
            overflow = (sum < INT32_MIN || sum > INT32_MAX);
            carry = ((sum & 0x100000000LL) != 0);
            break;
        }

        default:
            printf("[ERROR] ADD2 at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Write result to operand b (destination) - operands[0] is Destination per metadata */
    nd500_write_operand_value(cpu, &fi->operands[0], result, fi->data_type);

    /* Update status flags based on result (like C# lines 146-150) */
    nd500_set_flags_zsco(cpu, result, fi->data_type, carry, overflow);

    /* Handle trap conditions (like C# lines 152-164) */
    if (overflow) {
        printf("[TRAP] ADD2 at PC=0x%08X: Integer overflow\n", fi->address);
        trap_invalid_operation(cpu, fi->address);
        return;
    }
}
