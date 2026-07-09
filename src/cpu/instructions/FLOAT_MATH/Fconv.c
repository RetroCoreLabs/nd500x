#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdint.h>

/**
 * Fconv instruction - FLOAT_MATH class
 *
 * Converts source operand to 32-bit ND-500 single precision float.
 *
 * Variants: 5
 * Mnemonics: fconv fconv fconv fconv fconv
 * Operands: 2
 *
 * Opcodes:
 *   0xFD47 (fconv) - BI FCONV (bit to float)
 *   0xFD4C (fconv) - BY FCONV (byte to float)
 *   0xFD51 (fconv) - H FCONV (halfword to float)
 *   0xFD56 (fconv) - W FCONV (word to float)
 *   0xFD61 (fconv) - D FCONV (double to float)
 */
void nd500_instr_Fconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] FCONV expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t float_result = 0;

    /* Read source operand based on opcode */
    if (fi->opcode == 0xFD47) {
        /* BI FCONV: Bit to float */
        uint64_t bit_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
        int32_t int_val = (bit_val & 1) ? 1 : 0;
        float_result = nd500_float_from_int32(int_val);
    } else if (fi->opcode == 0xFD4C) {
        /* BY FCONV: Byte to float */
        uint64_t byte_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
        int32_t int_val = (int32_t)(int8_t)byte_val;  /* Sign extend */
        float_result = nd500_float_from_int32(int_val);
    } else if (fi->opcode == 0xFD51) {
        /* H FCONV: Halfword to float */
        uint64_t h_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_HALFWORD);
        int32_t int_val = (int32_t)(int16_t)h_val;  /* Sign extend */
        float_result = nd500_float_from_int32(int_val);
    } else if (fi->opcode == 0xFD56) {
        /* W FCONV: Word to float */
        uint64_t w_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
        int32_t int_val = (int32_t)w_val;
        float_result = nd500_float_from_int32(int_val);
    } else if (fi->opcode == 0xFD61) {
        /* D FCONV: Double to float */
        uint64_t double_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        float_result = nd500_double_to_single(double_bits);
    } else {
        printf("[ERROR] FCONV at PC=0x%08X: Unknown opcode 0x%04X\n",
               fi->address, fi->opcode);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Write result to destination operand as FLOAT so a register
     * destination goes to A1-A4, not I1-I4 */
    nd500_write_operand_value(cpu, &fi->operands[1], float_result, ND500_DTYPE_FLOAT);

    /* Set flags: Z (zero), S (sign) */
    if (nd500_float_is_zero(float_result)) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (nd500_float_is_negative(float_result)) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* O and C flags unaffected */
}
