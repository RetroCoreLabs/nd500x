#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Subc instruction - ARITHMETIC class
 *
 * Subtract with Carry: Rn + C - <subtrahend> - 1 → Rn
 *
 * Variants: 1 (word-only)
 * Mnemonics: Wn SUBC (n=1..4)
 * Operands: 1 (<subtrahend/r/t>)
 *
 * Opcodes:
 *   0xFE44-0xFE47 (W1 SUBC through W4 SUBC) - Word subtract with carry
 *
 * Operation: Rn + C - <subtrahend> - 1 → Rn
 *
 * Description:
 *   The carry bit (treated as 0 or 1) and the one's complement of
 *   <subtrahend> are added to the contents of the register. The result
 *   is stored back in the same register. Used for multiple-precision
 *   arithmetic.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if carry from most significant bit
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

    /* Get carry in (like C# line 55) */
    uint32_t carryIn = ((cpu->ST1 & ND500_FLAG_C) != 0) ? 1 : 0;

    /* SUBC: Rn + C + ~subtrahend (like C# lines 57-60) */
    /* This equals: Rn - subtrahend + C - 1 */
    uint32_t onesComplement = ~subtrahend;
    uint64_t result64 = (uint64_t)regValue + (uint64_t)onesComplement + (uint64_t)carryIn;
    uint32_t result = (uint32_t)result64;

    /* Write result back to register (like C# line 63) */
    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Detect carry out (like C# lines 65-66) */
    bool carryOut = (result64 > 0xFFFFFFFF);

    /* Detect overflow for signed subtraction (like C# line 69) */
    /* Overflow occurs when subtracting opposite signs produces wrong sign */
    int32_t signedReg = (int32_t)regValue;
    int32_t signedSub = (int32_t)subtrahend;
    int32_t signedResult = (int32_t)result;
    bool overflow = ((signedReg >= 0 && signedSub < 0 && signedResult < 0) ||
                     (signedReg < 0 && signedSub >= 0 && signedResult >= 0));

    /* Update status flags (like C# lines 71-75) */
    nd500_set_flags_zsco(cpu, result, ND500_DTYPE_WORD, carryOut, overflow);
}
