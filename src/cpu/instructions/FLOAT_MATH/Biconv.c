#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Biconv instruction - FLOAT_MATH class
 *
 * Variants: 5
 * Mnemonics: biconv biconv biconv biconv biconv
 * Operands: 2
 *
 * Opcodes:
 *   0xFD49 (biconv) - BY BICONV (byte to bit)
 *   0xFD4E (biconv) - H BICONV (halfword to bit)
 *   0xFD53 (biconv) - W BICONV (word to bit)
 *   0xFD58 (biconv) - F BICONV (float to bit)
 *   0xFD5D (biconv) - D BICONV (double to bit)
 *
 * Converts source operand to bit: non-zero -> 1, zero -> 0.
 * For floats, checks if value is non-zero (exponent != 0).
 */
void nd500_instr_Biconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] BICONV expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Convert source to bit: non-zero -> 1, zero -> 0 */
    bool bit_result = false;

    /* Read source operand based on opcode */
    if (fi->opcode == 0xFD49) {
        /* BY BICONV: Byte to bit - non-zero check */
        uint64_t byte_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        bit_result = (byte_val & 0xFF) != 0;
    } else if (fi->opcode == 0xFD4E) {
        /* H BICONV: Halfword to bit - non-zero check */
        uint64_t h_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_HALFWORD);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        bit_result = (h_val & 0xFFFF) != 0;
    } else if (fi->opcode == 0xFD53) {
        /* W BICONV: Word to bit - non-zero check */
        uint64_t w_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        bit_result = (w_val & 0xFFFFFFFF) != 0;
    } else if (fi->opcode == 0xFD58) {
        /* F BICONV: Float to bit - check if ND-500 float is non-zero.
         * Read as FLOAT so a register operand comes from A1-A4, not I1-I4. */
        uint32_t float_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        bit_result = !nd500_float_is_zero(float_bits);
    } else if (fi->opcode == 0xFD5D) {
        /* D BICONV: Double to bit - check if ND-500 double is non-zero */
        uint64_t double_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        bit_result = !nd500_double_is_zero(double_bits);
    } else {
        printf("[ERROR] BICONV at PC=0x%08X: Unknown opcode 0x%04X\n",
               fi->address, fi->opcode);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Write result to destination operand (0 or 1) */
    nd500_write_operand_value(cpu, &fi->operands[1], bit_result ? 1 : 0, ND500_DTYPE_BYTE);

    /* Set flags: Z (result is 0), S (result is 1) */
    if (bit_result) {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_set_flag(cpu, ND500_FLAG_Z);
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* O and C flags cleared */
    nd500_clear_flag(cpu, ND500_FLAG_O);
    nd500_clear_flag(cpu, ND500_FLAG_C);
}
