/*
 * Comp.c - ND-500 Comp instruction (COMPARE class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "float_exact.h"
#include <stdio.h>
#include <math.h>

/**
 * Comp instruction - COMPARE class
 *
 * Compare register with operand (subtract without storing result).
 * Rn - operand (result discarded, flags updated)
 *
 * Variants: 6 (by data type and register)
 * Mnemonics: BIn COMP, BYn COMP, Hn COMP, Wn COMP, Fn COMP, Dn COMP (n=1..4)
 * Operands: 1 (value to compare)
 *
 * Opcodes:
 *   0xFC18-0xFC1B (BI1 COMP through BI4 COMP) - Bit compare
 *   0x0030-0x0033 (BY1 COMP through BY4 COMP) - Byte compare
 *   0xFC1C-0xFC1F (H1 COMP through H4 COMP) - Halfword compare
 *   0x0034-0x0037 (W1 COMP through W4 COMP) - Word compare
 *   0x0038-0x003B (F1 COMP through F4 COMP) - Float compare (NOT IMPLEMENTED)
 *   0x003C-0x003F (D1 COMP through D4 COMP) - Double compare (NOT IMPLEMENTED)
 *
 * Operation: Rn - operand (result not stored)
 *
 * Description:
 *   The compare instruction subtracts the operand from the contents of the
 *   specified register. The result of the subtraction is not saved, but
 *   rather compared to zero, and this result is saved in the data status bits.
 *   The instruction is a true comparison, hence the sign bit is changed in
 *   case of integer overflow (S = sign XOR overflow).
 *
 * Flags: Z (zero), S (sign XOR overflow), C (carry/borrow)
 *   Z = 1 if result is zero (registers are equal)
 *   S = sign_bit XOR overflow (true comparison)
 *   C = 1 if NO borrow (reg >= operand for unsigned)
 *
 * Traps: Addressing traps only (integer), Floating overflow/underflow (float)
 *
 * Reference: ND-500 Reference Manual and docs/instructions/asm/ (authoritative).
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/COMPARE/Comp.cs
 */
void nd500_instr_Comp(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] COMP at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double variants */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        uint8_t reg_num = fi->target_register;

        if (reg_num < 1 || reg_num > 4) {
            printf("[ERROR] COMP at PC=0x%08X: Invalid register %u\n",
                   fi->address, reg_num);
            trap_illegal_operand(cpu, fi->address);
            return;
        }

        /* F COMP and D COMP are float subtracts: COMPF @002143 (AAP2,SUBBA TYP,F)
         * and COMPD @002147 (AAP2,SUBBA TYP,DF). An earlier version compared F
         * as integer bit patterns, following the reconstructed dispatch table
         * in the ND5000 engine, which sent F COMP to the integer entry 161; the
         * listing has the dedicated COMPF routine, and Ronny ruled on
         * 2026-09-19 that the listing routines decide. The difference is formed
         * exactly (float_exact.h): a host double keeps 53 of the 55 double
         * significand bits, so doubles differing only in their last two bits
         * compared equal. */
        uint64_t a = nd500_read_float_reg(cpu, reg_num, is_double);
        uint64_t b = nd500_read_float_operand(cpu, &fi->operands[0], is_double);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        unsigned exc = 0;
        uint64_t r = nd500_fx_sub(a, b, is_double, &exc);
        nd500_float_compare_status(cpu, r, exc, is_double);
        return;
    }

    /* Read register and operand (like C# ReadIntegerRegister + ReadOperandValue) */
    uint64_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t operand = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Normalise both to the datatype width before comparing. The register read
     * delivers the full register while the operand may be sign- or zero-extended
     * differently; comparing raw 64-bit values makes the carry (and, via an
     * unmasked subtraction, the borrow chain) wrong for sub-word datatypes when
     * the register's high bits are set. comp is a comparison OF THE DATATYPE.
     * (Z/sign already used nd500_mask_to_datatype below; this also fixes C.) */
    reg_value = nd500_mask_to_datatype(reg_value, fi->data_type);
    operand   = nd500_mask_to_datatype(operand, fi->data_type);

    /* BI (bit) COMPARE: isolate BOTH inputs to bit 0 (the single-bit value 0/1). The generic
     * mask_to_datatype leaves BI unmasked, so a register like 0xAAAAAAAA would compare its full
     * width instead of just its LSB. Mirrors RetroCore Comp.cs (mask = DataTypeWidthMask(BI) =
     * 0x01) so the executed-functional golden and this C core isolate the SAME bit. */
    if (fi->data_type == ND500_DTYPE_BIT) { reg_value &= 0x1ULL; operand &= 0x1ULL; }

    /* Perform subtraction (result not stored, only flags updated) */
    uint64_t result = reg_value - operand;

    /* Detect carry/borrow and overflow */
    /* ND-500 carry convention: C=1 means NO borrow (reg >= operand) */
    bool carry = (reg_value >= operand);
    bool overflow = nd500_detect_sub_overflow(reg_value, operand, result, fi->data_type);

    /* Mask to data type for flag calculations */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Determine sign bit from masked result */
    /* BI COMPARE sign = BYTE sign (0x80) of the RAW single-bit diff reg-op in {-1,0,+1}: the
     * microword COMP_BI @003234 / COMP2_BI @003241 subtract TYP,BY, so +1 (byte 0x01) -> S=0 and
     * -1 (byte 0xFF) -> S=1 = (reg<op). Taken from the unmasked , NOT masked_result (which
     * is bit-narrow), exactly as RetroCore Comp.cs uses (result & 0x80) for BI. */
    bool sign_bit = (fi->data_type == ND500_DTYPE_BIT)
                        ? ((result & 0x80ULL) != 0)
                        : nd500_is_negative(masked_result, fi->data_type);

    /* Update status flags: Z, S (XOR overflow for true comparison), C */
    /* Note: Result is NOT written back to register */
    if (masked_result == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (carry) {
        nd500_set_flag(cpu, ND500_FLAG_C);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_C);
    }

    /* S = sign_bit XOR overflow for true comparison */
    if (sign_bit ^ overflow) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }
}
