#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>

/**
 * Intr instruction - SYSTEM class
 *
 * INTR - Integer part WITH ROUNDING (manual ch.10.35)
 *
 * Format: tn INTR <x/r/t>
 *
 * | Assembly | Name                                  | Hex            | Octal          |
 * |----------|---------------------------------------|----------------|----------------|
 * | Fn INTR  | float integer part with rounding      | 0FE68H+(n-1)   | 177150B+(n-1)  |
 * | Dn INTR  | double float integer part with rounding | 0FE6CH+(n-1) | 177154B+(n-1)  |
 *
 * Operation: rounded integer part of <x> in FLOAT format -> Rn
 *
 * Trap conditions: Addressing traps. NOT privileged.
 *
 * Data status bits:
 *   result = 0        -> Z
 *   result.signbit    -> S
 *   (O and C unaffected)
 *
 * This is the rounding sibling of INT (ch.10.34, 0xFE60-0xFE67), which
 * truncates. The two differ only in that step.
 *
 * ---------------------------------------------------------------------------
 * REWRITTEN 2026-08-09 - this was the WRONG INSTRUCTION ENTIRELY.
 *
 * It used to implement "read the interrupt request register", return a
 * hardcoded 0, and require privilege. Nothing about that is INTR. The old
 * header even documented the operands as "Hn INTR"/"Wn INTR" (halfword/word)
 * when the instruction is float/double.
 *
 * Three independent sources say so, and they agree:
 *   1. Manual ch.10.35 - the entry quoted above.
 *   2. The dispatch table in this repo, src/cpu/nd500_instructions.c:342-349,
 *      maps 0xFE68-0xFE6B and 0xFE6C-0xFE6F, which is exactly the manual's
 *      Fn/Dn INTR opcode range.
 *   3. The ND-5000 control store. Its label file has INTRF (002613) and INTRD
 *      (002615) as the float and double routines, and RetroCore's dispatch map
 *      generated from that same .LABE reads
 *      "map[65128] = new DispatchEntry(1419, 0, 1); // F1 INTR" - 65128 = 0xFE68.
 *
 * Consequence of the old code: any unprivileged program using Fn INTR took an
 * IIC trap instead of getting a rounded float, and any privileged one silently
 * got 0. There was no corpus coverage - a search of all 40088 cases found zero
 * INTR tests - which is why this survived. Corpus cases are specified in
 * docs/SPEC-CORPUS-SOLO-TUTTI-DT-DE.md's companion entry for INTR.
 *
 * ROUNDING MODE - INFERRED, NOT PROVEN.
 * The manual says only "The result is rounded." round() below is
 * round-half-away-from-zero. The alternative worth considering is
 * round-half-to-even. The microcode distinguishes the two instructions exactly
 * at the step where they must differ:
 *     020335 INTF_1:  ALU,FZRO A,BM00 B,X1 D,SC6   (truncating sibling)
 *     020345 INTRF_1: ALU,A-1  A,BM26 B,X1 D,SC2   (this instruction)
 * BM26 is octal, so it is bit 22; A-1 makes a 22-bit mask. Tracing INTRF_0
 * (020341) and INTRD_0 (020361) to their returns would settle the mode. That
 * was not done here, so any test asserting the .5 cases must treat the
 * half-way behaviour as unverified rather than baking this choice in.
 * ---------------------------------------------------------------------------
 */
void nd500_instr_Intr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] INTR expects 1 operand, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Double variants are the upper half of the opcode range, exactly as in
     * Int.c: 0xFE68-0xFE6B are Fn, 0xFE6C-0xFE6F are Dn. */
    bool is_double = (fi->opcode >= 0xFE6C && fi->opcode <= 0xFE6F);
    uint8_t reg_num = fi->target_register;

    if (reg_num < 1 || reg_num > 4) {
        printf("[ERROR] INTR at PC=0x%08X: Invalid register %u\n",
               fi->address, reg_num);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    double argument = 0.0;
    double rounded = 0.0;
    uint64_t result_bits = 0;

    if (is_double) {
        uint64_t double_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. Same guard as Int.c and ADD3 (commit a351296). */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        argument = nd500_double_to_ieee754(double_bits);
    } else {
        uint32_t float_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        argument = (double)nd500_float_to_ieee754(float_bits);
    }

    /* The one line that differs from INT, which uses trunc(). See the rounding
     * note in the header - the half-way case is not verified. */
    rounded = round(argument);

    /* Result goes back in FLOAT format, not integer format. */
    if (is_double) {
        result_bits = nd500_double_from_ieee754(rounded);
        nd500_write_double_register(cpu, reg_num, result_bits);
    } else {
        result_bits = nd500_float_from_ieee754((float)rounded);
        nd500_write_float_register(cpu, reg_num, (uint32_t)result_bits);
    }

    /* Z from the result being zero, S from its sign bit. */
    if (is_double) {
        if (nd500_double_is_zero(result_bits)) {
            nd500_set_flag(cpu, ND500_FLAG_Z);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_Z);
        }
        if (nd500_double_is_negative(result_bits)) {
            nd500_set_flag(cpu, ND500_FLAG_S);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_S);
        }
    } else {
        uint32_t f = (uint32_t)result_bits;
        if (nd500_float_is_zero(f)) {
            nd500_set_flag(cpu, ND500_FLAG_Z);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_Z);
        }
        if (nd500_float_is_negative(f)) {
            nd500_set_flag(cpu, ND500_FLAG_S);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_S);
        }
    }

    /* O and C unaffected. */
}
