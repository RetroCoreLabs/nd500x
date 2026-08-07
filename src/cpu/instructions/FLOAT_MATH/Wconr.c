#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

/**
 * Wconr instruction - FLOAT_MATH class
 *
 * Variants: 2
 * Mnemonics: wconr wconr
 * Operands: 2
 *
 * Opcodes:
 *   0xFE74 (wconr) - F WCONR (float to word with rounding)
 *   0xFE75 (wconr) - D WCONR (double to word with rounding)
 *
 * Converts float/double source to word with rounding (truncate toward zero).
 * Traps IOV if value outside word range.
 */
void nd500_instr_Wconr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] WCONR expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine source type from opcode */
    bool is_double = (fi->opcode == 0xFE75);
    int64_t source_value = 0;
    int32_t word_result = 0;
    bool overflow = false;

    /* Read and convert source operand */
    if (is_double) {
        /* D WCONR: Double to word (truncate toward zero) */
        uint64_t double_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        source_value = nd500_double_to_int64(double_bits);
        if (source_value < INT32_MIN || source_value > INT32_MAX) {
            overflow = true;
        }
    } else {
        /* F WCONR: Float to word (truncate toward zero).
         * Read as FLOAT so a register operand comes from A1-A4, not I1-I4. */
        uint32_t float_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        int32_t int_val = nd500_float_to_int32(float_bits);
        source_value = int_val;
        /* Float to int32 can't overflow int32 range */
    }

    /* Check for overflow trap */
    if (overflow) {
        ND500X_TRAPLOG("[TRAP] WCONR at PC=0x%08X: Value %lld outside word range\n",
               fi->address, (long long)source_value);
        raise_trap(cpu, TRAP_O, fi->address, 0);
        return;
    }

    /* Convert to word (truncate) */
    word_result = (int32_t)source_value;

    /* Write result to destination operand */
    nd500_write_operand_value(cpu, &fi->operands[1], (uint64_t)(uint32_t)word_result, ND500_DTYPE_WORD);

    /* Set flags: Z (zero), S (sign) */
    if (word_result == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (word_result < 0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* O and C flags cleared */
    nd500_clear_flag(cpu, ND500_FLAG_O);
    nd500_clear_flag(cpu, ND500_FLAG_C);
}
