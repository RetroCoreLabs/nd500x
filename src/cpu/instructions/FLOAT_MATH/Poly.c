#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * POLY instruction - FLOAT_MATH class
 *
 * Evaluates a polynomial using Horner's method:
 *   result = c[m]*x^m + c[m-1]*x^(m-1) + ... + c[1]*x + c[0]
 *
 * Horner's algorithm (more efficient):
 *   result = c[m]
 *   for i = m-1 down to 0:
 *       result = result * x + c[i]
 *
 * Opcodes:
 *   0xFCE0-0xFCE3: Fn POLY (single-precision float, F1-F4)
 *   0xFCE4-0xFCE7: Dn POLY (double-precision float, D1-D4)
 *
 * Operands:
 *   Operand 0: X value (independent variable)
 *   Operand 1: Degree m (constant byte, 0-255)
 *   Extra operands: m+1 coefficients in cpu->extra_operands[] (c[m], c[m-1], ..., c[0])
 *
 * Status flags:
 *   Z: Set if result == 0.0
 *   S: Set if result < 0.0
 *   FO: Set if floating overflow occurred
 *   FU: Set if floating underflow occurred
 */
void nd500_instr_Poly(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* 1. Validate operand count: 2 fixed operands (x, degree) plus the
     * coefficients, which the decoder appends to fi->operands AND stores
     * in cpu->extra_operands */
    if (fi->operand_count < 2) {
        printf("[ERROR] POLY at PC=0x%08X: Expected >=2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* 2. Determine data type (F=float, D=double) from data_type */
    bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);

    /* 3. Read X value (operand 0) */
    uint64_t x_bits = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* 4. Read degree m (operand 1 - must be constant byte) */
    uint8_t m = nd500_read_operand_byte(cpu, &fi->operands[1]);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* 5. Validate coefficient count (m+1 coefficients expected) */
    uint16_t expected_coeffs = (uint16_t)m + 1;
    if (cpu->extra_operand_count != expected_coeffs) {
        printf("[ERROR] POLY at PC=0x%08X: Expected %u coefficients (degree %u), got %u\n",
               fi->address, expected_coeffs, m, cpu->extra_operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* 6. Convert X to IEEE 754 for arithmetic */
    double x;
    if (is_double) {
        x = nd500_double_to_ieee754(x_bits);
    } else {
        x = (double)nd500_float_to_ieee754((uint32_t)x_bits);
    }

    /* 7. Horner's method evaluation
     *
     * Coefficients are stored in order: c[m], c[m-1], ..., c[1], c[0]
     * extra_operands[0] = c[m]   (highest degree coefficient)
     * extra_operands[1] = c[m-1]
     * ...
     * extra_operands[m] = c[0]   (constant term)
     */
    double result = 0.0;
    bool overflow = false;
    bool underflow = false;

    for (uint16_t i = 0; i <= m; i++) {
        /* Read coefficient from extra_operands */
        uint64_t coeff_bits = nd500_read_operand_value(cpu,
            &cpu->extra_operands[i], fi->data_type);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        double coeff;
        if (is_double) {
            coeff = nd500_double_to_ieee754(coeff_bits);
        } else {
            coeff = (double)nd500_float_to_ieee754((uint32_t)coeff_bits);
        }

        /* Horner step */
        if (i == 0) {
            result = coeff;  /* First iteration: result = c[m] */
        } else {
            result = result * x + coeff;  /* result = result*x + c[m-i] */
        }

        /* Check for overflow/underflow (delayed until end per ND-500 spec) */
        if (isinf(result)) {
            overflow = true;
        }
        /* Check for underflow: non-zero result smaller than smallest normal double */
        if (result != 0.0 && !isinf(result) && fabs(result) < 2.2250738585072014e-308) {
            underflow = true;
        }
    }

    /* 8. Handle overflow: per ND-500 spec, result is set to 0 on overflow */
    if (overflow) {
        result = 0.0;
    }

    /* 9. Convert result back to ND500 format and write to register */
    if (is_double) {
        uint64_t result_bits = nd500_double_from_ieee754(result);
        nd500_write_double_register(cpu, fi->target_register, result_bits);
    } else {
        uint32_t result_bits = nd500_float_from_ieee754((float)result);
        nd500_write_float_register(cpu, fi->target_register, result_bits);
    }

    /* 10. Set status flags */
    nd500_clear_flag(cpu, ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_FO | ND500_FLAG_FU);

    if (result == 0.0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    }
    if (result < 0.0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    }
    if (overflow) {
        nd500_set_flag(cpu, ND500_FLAG_FO);
    }
    if (underflow) {
        nd500_set_flag(cpu, ND500_FLAG_FU);
    }
}
