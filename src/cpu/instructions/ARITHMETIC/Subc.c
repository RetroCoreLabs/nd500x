#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Subc instruction - ARITHMETIC class
 *
 * Subtract with Carry/Borrow: Rn - <subtrahend> - C → Rn
 *
 * Variants: 1 (word-only)
 * Mnemonics: Wn SUBC (n=1..4)
 * Operands: 1 (<subtrahend/r/t>)
 *
 * Opcodes:
 *   0xFE44-0xFE47 (W1 SUBC through W4 SUBC) - Word subtract with carry
 *
 * Operation: Rn - <subtrahend> - C → Rn (Intel convention: C=borrow)
 *
 * Description:
 *   Subtracts the subtrahend and the borrow flag from the register.
 *   Intel convention: C=1 means borrow from previous operation.
 *   Used for multiple-precision subtraction.
 *
 * Flags: Z (zero), S (sign), C (carry/borrow), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if borrow occurred (Intel convention)
 *   O = 1 if signed overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *
 * Reference: ND-500 Reference Manual, Chapter 11.18
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Subc.cs
 */
void nd500_instr_Subc(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 46-50) */
    if (fi->operand_count != 1) {
        printf("[ERROR] SUBC at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read register value (like C# line 53) */
    uint32_t regValue = nd500_read_integer_register(cpu, fi->target_register);

    /* Read operand value (like C# line 54) */
    uint32_t subtrahend = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);

    /* Get borrow in - Intel convention: C=1 means borrow from previous operation */
    uint32_t borrowIn = ((cpu->ST1 & ND500_FLAG_C) != 0) ? 1 : 0;

    /* SUBC: Rn - subtrahend - borrowIn (Intel convention) */
    /* Using ones' complement: Rn + ~subtrahend + (1 - borrowIn) */
    uint32_t onesComplement = ~subtrahend;
    uint64_t result64 = (uint64_t)regValue + (uint64_t)onesComplement + (uint64_t)(1 - borrowIn);
    uint32_t result = (uint32_t)result64;

    /* Write result back to register */
    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Detect borrow out - Intel convention: C=1 if borrow occurred */
    /* Borrow occurs if regValue < subtrahend + borrowIn (with wrap check) */
    bool borrowOut = (regValue < subtrahend) || (regValue == subtrahend && borrowIn == 1);

    /* Detect overflow for signed subtraction (like C# line 69) */
    /* Overflow occurs when subtracting opposite signs produces wrong sign */
    int32_t signedReg = (int32_t)regValue;
    int32_t signedSub = (int32_t)subtrahend;
    int32_t signedResult = (int32_t)result;
    bool overflow = ((signedReg >= 0 && signedSub < 0 && signedResult < 0) ||
                     (signedReg < 0 && signedSub >= 0 && signedResult >= 0));

    /* Update status flags - Intel convention: C = borrowOut */
    nd500_set_flags_zsco(cpu, result, ND500_DTYPE_WORD, borrowOut, overflow);
}
