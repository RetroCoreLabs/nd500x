#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Mulad instruction - ARITHMETIC class
 *
 * Multiply and Add: Rn * <x> + <y> → Rn
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn MULAD, Hn MULAD, Wn MULAD, Fn MULAD, Dn MULAD (n=1..4)
 * Operands: 2 (<x/r/t>, <y/r/t>)
 *
 * Opcodes:
 *   0xFCE8-0xFCEB (BY1 MULAD through BY4 MULAD) - Byte multiply and add
 *   0xFCEC-0xFCEF (H1 MULAD through H4 MULAD) - Halfword multiply and add
 *   0x00A8-0x00AB (W1 MULAD through W4 MULAD) - Word multiply and add
 *   0xFCF0-0xFCF3 (F1 MULAD through F4 MULAD) - Float multiply and add
 *   0xFCF4-0xFCF7 (D1 MULAD through D4 MULAD) - Double multiply and add
 *
 * Operation: Rn * <x> + <y> → Rn
 *
 * Description:
 *   The register contents are multiplied by <x>, <y> is added to
 *   the product, and the result is stored in the register.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow), FO, FU
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if carry from most significant bit (integer word only)
 *   O = 1 if overflow
 *   FO = 1 if floating overflow
 *   FU = 1 if floating underflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11.19
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Mulad.cs
 */
void nd500_instr_Mulad(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 53-57) */
    if (fi->operand_count != 2) {
        printf("[ERROR] MULAD at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double types.
     * Microcode MULADF @002633 / MULADD @002635: Rn * <x> + <y> -> Rn;
     * ST,SAVF/ST,ACCF set Z,S,FU,FO; C,O cleared (rule 4040). */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        double regValue = is_double
            ? nd500_double_to_ieee754(nd500_read_double_register(cpu, fi->target_register))
            : (double)nd500_float_to_ieee754(nd500_read_float_register(cpu, fi->target_register));
        double x = nd500_read_operand_as_ieee_float(cpu, &fi->operands[0], is_double);
        double y = nd500_read_operand_as_ieee_float(cpu, &fi->operands[1], is_double);
        double fresult = regValue * x + y;
        uint64_t bits = nd500_float_finish(cpu, fi->address, fresult, is_double);
        if (is_double) {
            nd500_write_double_register(cpu, fi->target_register, bits);
        } else {
            nd500_write_float_register(cpu, fi->target_register, (uint32_t)bits);
        }
        return;
    }

    /* Integer multiply and add (like C# lines 145-202) */

    /* Read register value (like C# line 146) */
    uint32_t regValue = nd500_read_integer_register(cpu, fi->target_register);

    /* Read operand x (like C# line 147) */
    uint64_t x = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Read operand y (like C# line 148) */
    uint64_t y = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Perform: Rn * x + y (like C# lines 150-191) */
    uint64_t result = 0;
    bool overflow = false;
    bool carry = false;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            /* Signed byte multiply and add (like C# lines 157-166) */
            int8_t reg = (int8_t)(regValue & 0xFF);
            int8_t xVal = (int8_t)(x & 0xFF);
            int8_t yVal = (int8_t)(y & 0xFF);
            int32_t temp = reg * xVal + yVal;    /* (like C# line 162) */
            result = (uint64_t)(uint8_t)temp;    /* (like C# line 163) */
            overflow = (temp < INT8_MIN || temp > INT8_MAX);  /* (like C# line 164) */
            break;
        }

        case ND500_DTYPE_HALFWORD: {
            /* Signed halfword multiply and add (like C# lines 167-176) */
            int16_t reg = (int16_t)(regValue & 0xFFFF);
            int16_t xVal = (int16_t)(x & 0xFFFF);
            int16_t yVal = (int16_t)(y & 0xFFFF);
            int32_t temp = reg * xVal + yVal;    /* (like C# line 172) */
            result = (uint64_t)(uint16_t)temp;   /* (like C# line 173) */
            overflow = (temp < INT16_MIN || temp > INT16_MAX);  /* (like C# line 174) */
            break;
        }

        case ND500_DTYPE_WORD: {
            /* Signed word multiply and add (like C# lines 177-187) */
            int32_t reg = (int32_t)(regValue & 0xFFFFFFFF);
            int32_t xVal = (int32_t)(x & 0xFFFFFFFF);
            int32_t yVal = (int32_t)(y & 0xFFFFFFFF);
            int64_t temp = (int64_t)reg * (int64_t)xVal + (int64_t)yVal;  /* (like C# line 182) */
            result = (uint64_t)(uint32_t)temp;   /* (like C# line 183) */
            overflow = (temp < INT32_MIN || temp > INT32_MAX);  /* (like C# line 184) */
            carry = (temp > 0xFFFFFFFF);         /* (like C# line 185) */
            break;
        }

        default:
            printf("[ERROR] MULAD at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Mask and write result (like C# lines 194-195) */
    uint32_t maskedResult = nd500_mask_to_datatype((uint32_t)result, fi->data_type);
    nd500_write_integer_register(cpu, fi->target_register, maskedResult);

    /* Update status flags (like C# lines 198-201) */
    /* Set Z flag based on result */
    if (maskedResult == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    /* Set S flag based on sign bit */
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

    /* Set C flag (only for word operations) */
    if (carry) {
        cpu->ST1 |= ND500_FLAG_C;
    } else {
        cpu->ST1 &= ~ND500_FLAG_C;
    }

    /* Set O flag based on overflow */
    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    /* Handle trap on overflow */
    if (overflow) {
        printf("[TRAP] MULAD at PC=0x%08X: Integer overflow\n", fi->address);
        trap_invalid_operation(cpu, fi->address);
        return;
    }
}
