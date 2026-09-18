#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Addc instruction - ARITHMETIC class
 *
 * Add with Carry: Rn + C + <addend> -> Rn
 *
 * Variants: 1 (word-only)
 * Mnemonics: Wn ADDC (n=1..4)
 * Operands: 1 (<addend/r/t>)
 *
 * Opcodes:
 *   0xFE40-0xFE43 (W1 ADDC through W4 ADDC) - Word add with carry
 *
 * Operation: Rn + C + <addend> -> Rn
 *
 * Description:
 *   The <addend> operand, the carry bit (interpreted as 0 or 1), and
 *   the contents of the specified register are added. The result is
 *   stored in the register. This instruction is used for multiple-precision
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
 * Reference: ND-500 Reference Manual, Chapter 11.17
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Addc.cs
 */
void nd500_instr_Addc(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 46-50) */
    if (fi->operand_count != 1) {
        printf("[ERROR] ADDC at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read register value (like C# line 53) */
    uint32_t regValue = nd500_read_integer_register(cpu, fi->target_register);

    /* Read operand value (like C# line 54) */
    uint32_t addend = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Get carry in (like C# line 55) */
    uint32_t carryIn = ((cpu->ST1 & ND500_FLAG_C) != 0) ? 1 : 0;

    /* Perform addition with carry (like C# lines 57-59) */
    uint64_t result64 = (uint64_t)regValue + (uint64_t)addend + (uint64_t)carryIn;
    uint32_t result = (uint32_t)result64;

    /* Write result back to register (like C# line 62) */
    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Detect carry out (like C# lines 64-65) */
    bool carryOut = (result64 > 0xFFFFFFFF);

    /* Detect overflow for signed addition (like C# line 68) */
    /* Overflow occurs when two same-sign operands produce opposite-sign result */
    int32_t signedReg = (int32_t)regValue;
    int32_t signedAddend = (int32_t)(addend + carryIn);
    int32_t signedResult = (int32_t)result;
    bool overflow = ((signedReg > 0 && signedAddend > 0 && signedResult < 0) ||
                     (signedReg < 0 && signedAddend < 0 && signedResult > 0));

    /* Update status flags (like C# lines 70-74) */
    nd500_set_flags_zsco(cpu, result, ND500_DTYPE_WORD, carryOut, overflow);
}
