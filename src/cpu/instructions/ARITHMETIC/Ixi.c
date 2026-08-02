#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * IXI instruction - ARITHMETIC class
 *
 * I to the J'th Power (Integer Exponentiation): <i> ** <j> -> Rn
 *
 * Mnemonics: BYn IXI, Hn IXI, Wn IXI (n=1..4)
 * Operands: 2 (<i/r/t>, <j/r/t>)
 *
 * Opcodes:
 *   0xFCC8-0xFCCB (BY1 IXI through BY4 IXI) - Byte
 *   0xFCCC-0xFCCF (H1 IXI through H4 IXI) - Halfword
 *   0xFCD0-0xFCD3 (W1 IXI through W4 IXI) - Word
 *
 * Operation: <i> ** <j> -> Rn (i raised to power j)
 *
 * Special cases:
 *   - Any number to power 0 = 1
 *   - 0 to negative power = illegal operand trap
 *   - 1 to any power = 1
 *   - -1 to power j = 1 if j even, -1 if j odd
 *   - Other bases to negative power = 0 (integer truncation)
 *
 * Flags: Z (zero), S (sign), O (overflow)
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
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Sign-extend operands based on data type */
    int64_t base_val, exponent;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            base_val = (int8_t)(i_val & 0xFF);
            exponent = (int8_t)(j_val & 0xFF);
            break;
        case ND500_DTYPE_HALFWORD:
            base_val = (int16_t)(i_val & 0xFFFF);
            exponent = (int16_t)(j_val & 0xFFFF);
            break;
        case ND500_DTYPE_WORD:
        default:
            base_val = (int32_t)i_val;
            exponent = (int32_t)j_val;
            break;
    }

    /* Calculate: base ** exponent (integer exponentiation) */
    int64_t result = 0;
    bool overflow = false;
    bool illegal_operand = false;

    if (exponent == 0) {
        /* Any number to power 0 is 1 */
        result = 1;
    } else if (exponent < 0) {
        /* Negative exponent handling */
        if (base_val == 0) {
            /* 0 to negative power is undefined - trap */
            illegal_operand = true;
            result = 0;
        } else if (base_val == 1) {
            result = 1;
        } else if (base_val == -1) {
            /* -1 to even power = 1, -1 to odd power = -1 */
            result = (exponent % 2 == 0) ? 1 : -1;
        } else {
            /* Non-unit base to negative power = 0 (integer truncation) */
            result = 0;
        }
    } else {
        /* Positive exponent - calculate power by repeated multiplication */
        result = 1;
        for (int64_t i = 0; i < exponent; i++) {
            /* Check for overflow before multiplication */
            int64_t prev_result = result;
            result *= base_val;

            /* Detect overflow based on data type limits */
            switch (fi->data_type) {
                case ND500_DTYPE_BYTE:
                    if (result < -128 || result > 127) overflow = true;
                    break;
                case ND500_DTYPE_HALFWORD:
                    if (result < -32768 || result > 32767) overflow = true;
                    break;
                case ND500_DTYPE_WORD:
                default:
                    if (result < INT32_MIN || result > INT32_MAX) overflow = true;
                    break;
            }
        }
    }

    /* Mask result to data type size */
    uint32_t masked_result;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            masked_result = (uint32_t)(result & 0xFF);
            break;
        case ND500_DTYPE_HALFWORD:
            masked_result = (uint32_t)(result & 0xFFFF);
            break;
        case ND500_DTYPE_WORD:
        default:
            masked_result = (uint32_t)result;
            break;
    }

    /* Write result to register */
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    /* Update status flags: Z, S, O (no C for IXI) */
    nd500_clear_flag(cpu, ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O);

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
    if (overflow) {
        nd500_set_flag(cpu, ND500_FLAG_O);
    }

    /* Handle trap conditions */
    if (illegal_operand) {
        trap_invalid_operation(cpu, fi->address);
    } else if (overflow) {
        trap_invalid_operation(cpu, fi->address);
    }
}
