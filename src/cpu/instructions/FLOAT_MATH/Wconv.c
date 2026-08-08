#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdint.h>

/**
 * Wconv instruction - FLOAT_MATH class
 *
 * Converts source operand to 32-bit signed integer (word).
 *
 * Variants: 5
 * Mnemonics: wconv wconv wconv wconv wconv
 * Operands: 2
 *
 * Opcodes:
 *   0xFD46 (wconv) - BI WCONV (bit to word) - trivial, 0 or 1
 *   0xFD4B (wconv) - BY WCONV (byte to word) - sign extend
 *   0xFD50 (wconv) - H WCONV (halfword to word) - sign extend
 *   0xFD5B (wconv) - F WCONV (float to word) - truncate toward zero
 *   0xFD60 (wconv) - D WCONV (double to word) - truncate toward zero
 */
void nd500_instr_Wconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] WCONV expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    int32_t word_result = 0;
    bool overflow = false;   /* D WCONV out of word range - see below */

    /* Read source operand based on opcode */
    if (fi->opcode == 0xFD46) {
        /* BI WCONV: Bit to word */
        uint64_t bit_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BIT);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        word_result = (bit_val & 1) ? 1 : 0;
    } else if (fi->opcode == 0xFD4B) {
        /* BY WCONV: Byte to word (sign extend) */
        uint64_t byte_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        word_result = (int32_t)(int8_t)byte_val;
    } else if (fi->opcode == 0xFD50) {
        /* H WCONV: Halfword to word (sign extend) */
        uint64_t h_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_HALFWORD);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        word_result = (int32_t)(int16_t)h_val;
    } else if (fi->opcode == 0xFD5B) {
        /* F WCONV: Float to word (truncate toward zero).
         * Read as FLOAT so a register operand comes from A1-A4, not I1-I4. */
        uint32_t float_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        word_result = nd500_float_to_int32(float_bits);
    } else if (fi->opcode == 0xFD60) {
        /* D WCONV: Double to word (truncate toward zero) */
        uint64_t double_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        int64_t int64_result = nd500_double_to_int64(double_bits);
        /* Out of word range: clamp, set O, and raise integer overflow.
         *
         * This used to clamp silently - no trap, and not even the O flag that
         * every sibling conversion sets. Manual 15.2: "Conversion of longer to
         * shorter data types is by truncation of the most significant bits and
         * may cause integer overflow." Corroborated by RetroCore's Wconv.cs,
         * which already raises integer overflow here.
         *
         * O is bit 9, which NDIX does NOT arm, so the guest ignores this - the
         * same reason BYCONV's trap was safe to restore. */
        if (int64_result > INT32_MAX) {
            word_result = INT32_MAX;
            overflow = true;
        } else if (int64_result < INT32_MIN) {
            word_result = INT32_MIN;
            overflow = true;
        } else {
            word_result = (int32_t)int64_result;
        }
    } else {
        printf("[ERROR] WCONV at PC=0x%08X: Unknown opcode 0x%04X\n",
               fi->address, fi->opcode);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Write result to destination operand */
    nd500_write_operand_value(cpu, &fi->operands[1], (uint32_t)word_result, ND500_DTYPE_WORD);

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

    /* Integer overflow: the clamped result IS written (manual 15.2 - conversion
     * is by truncation and may cause integer overflow), the O flag is set, and
     * then the trap is raised. C is unaffected. */
    if (overflow) {
        nd500_set_flag(cpu, ND500_FLAG_O);
        trap_integer_overflow(cpu, fi->address);
        return;
    }
    nd500_clear_flag(cpu, ND500_FLAG_O);

    /* C flag unaffected */
}
