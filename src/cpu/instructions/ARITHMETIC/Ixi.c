#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * IXI instruction - ARITHMETIC class
 *
 * Index calculation: <i> * <j> -> Rn
 *
 * Despite the reference manual saying "I to the J'th power", the actual
 * operation is multiplication for array index calculation:
 *   result = index * element_size
 *
 * Mnemonics: BYn IXI, Hn IXI, Wn IXI (n=1..4)
 * Operands: 2 (<i/r/t>, <j/r/t>)
 *
 * Opcodes:
 *   0xFCC8-0xFCCB (BY1 IXI through BY4 IXI) - Byte
 *   0xFCCC-0xFCCF (H1 IXI through H4 IXI) - Halfword
 *   0xFCD0-0xFCD3 (W1 IXI through W4 IXI) - Word
 *
 * Operation: <i> * <j> -> Rn (datatype dependent part)
 *
 * Flags: Z (zero), S (sign), O (overflow), C (carry)
 *
 * Reference: ND-500 Reference Manual, Chapter 12.2
 */
void nd500_instr_Ixi(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] IXI at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands */
    uint64_t i_val = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    uint64_t j_val = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Sign-extend for signed multiplication */
    int64_t i_signed, j_signed;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            i_signed = (int8_t)(i_val & 0xFF);
            j_signed = (int8_t)(j_val & 0xFF);
            break;
        case ND500_DTYPE_HALFWORD:
            i_signed = (int16_t)(i_val & 0xFFFF);
            j_signed = (int16_t)(j_val & 0xFFFF);
            break;
        case ND500_DTYPE_WORD:
        default:
            i_signed = (int32_t)i_val;
            j_signed = (int32_t)j_val;
            break;
    }

    /* Calculate: i * j */
    int64_t result = i_signed * j_signed;

    /* Check for overflow based on data type */
    bool overflow = false;
    bool carry = false;
    uint32_t masked_result;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            overflow = (result < -128 || result > 127);
            carry = ((uint64_t)result > 0xFF);
            masked_result = (uint32_t)(result & 0xFF);
            break;
        case ND500_DTYPE_HALFWORD:
            overflow = (result < -32768 || result > 32767);
            carry = ((uint64_t)result > 0xFFFF);
            masked_result = (uint32_t)(result & 0xFFFF);
            break;
        case ND500_DTYPE_WORD:
        default:
            overflow = (result < INT32_MIN || result > INT32_MAX);
            carry = ((uint64_t)result > 0xFFFFFFFF);
            masked_result = (uint32_t)result;
            break;
    }

    /* Write result to register */
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    /* Update status flags */
    nd500_clear_flag(cpu, ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O);

    if (masked_result == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    }

    /* Check sign bit based on data type */
    uint32_t sign_mask;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            sign_mask = 0x80;
            break;
        case ND500_DTYPE_HALFWORD:
            sign_mask = 0x8000;
            break;
        case ND500_DTYPE_WORD:
        default:
            sign_mask = 0x80000000;
            break;
    }

    if (masked_result & sign_mask) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    }
    if (carry) {
        nd500_set_flag(cpu, ND500_FLAG_C);
    }
    if (overflow) {
        nd500_set_flag(cpu, ND500_FLAG_O);
    }
}
