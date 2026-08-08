#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Test instruction - COMPARE class
 *
 * Test operand against zero (compare with implicit zero).
 *
 * Variants: 6 (by data type)
 * Mnemonics: BI TEST, BY TEST, H TEST, W TEST, F TEST, D TEST
 * Operands: 1 (operand to test)
 *
 * Opcodes:
 *   0x0041 (BI TEST) - Bit test against zero
 *   0x0042 (BY TEST) - Byte test against zero
 *   0x0043 (H TEST) - Halfword test against zero
 *   0x0044 (W TEST) - Word test against zero
 *   0x0045 (F TEST) - Float test against zero
 *   0x0046 (D TEST) - Double test against zero
 *
 * Operation: operand - 0 (result discarded, flags updated)
 *
 * Description:
 *   This instruction is similar to comparing two operands, except that
 *   the second operand is implicitly zero. The operand is tested against
 *   zero and flags are updated accordingly.
 *
 * Flags: Z (zero), S (sign), C (carry)
 *   Z = 1 if operand is zero
 *   S = 1 if operand sign bit is set (no overflow for test against zero)
 *   C = 1 for BY/H/W integer TEST; C = 0 for BI TEST (microcode CS 003243 SC5-256
 *       always borrows -> C=0, Ronny-adjudicated over manual 10.11); float TEST no C
 *
 * Trap conditions: Addressing traps only
 *
 * Reference: ND-500 Reference Manual, Chapter 10.11
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/COMPARE/Test.cs
 */
void nd500_instr_Test(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] TEST at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operand */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Test against zero (result = value - 0 = value).
     * Microcode F TEST @000252 / D TEST @000253 (ST,SAVC): Z = (operand == 0.0),
     * S = signbit(operand); float TEST does NOT set C. Integer TEST @000250
     * (ST,SAVC): Z = (operand == 0), S = signbit, C = 1 ALWAYS (subtracting 0
     * never borrows). O is not listed for either -> CLEARED (rule 4040). */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        nd500_set_flags_zs_float(cpu, value, is_double);
    } else {
        nd500_set_flags_zs(cpu, value, fi->data_type);
        /* C flag for integer test-against-zero.
         *
         * EXCEPTION - BIT (BI) test sets NO carry (C=0), NOT 1. The real ND-5000 B30
         * microcode is ground truth: the TEST_BI tail at CS 003243 is
         *   ALU,B-A CRY,ONE A,BM10(=256) B,SC5 ST,SAVC
         * i.e. it computes SC5(0 or 1) - 256, which ALWAYS borrows, so C=0. The manual
         * 10.11 generic "1 -> C (integer)" does NOT hold for the BI width - the microcode
         * wins (Ronny-adjudicated over manual 10.11, LIND-style verify-vs-microcode).
         * Scoped to BI ONLY: BY/H/W keep the integer "1 -> C". COMP-BI is UNCHANGED - its
         * microcode (CS 003234 SC6-SC5) keeps the data-dependent reg>=operand carry. */
        if (fi->data_type == ND500_DTYPE_BIT) {
            nd500_clear_flag(cpu, ND500_FLAG_C);
        } else {
            nd500_set_flag(cpu, ND500_FLAG_C);
        }
    }
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
