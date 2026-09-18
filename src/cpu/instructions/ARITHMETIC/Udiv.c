#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Udiv instruction - ARITHMETIC class
 *
 * Unsigned Divide: <a> / <b> -> <c>, remainder -> Rn
 *
 * Variants: 1 (word-only)
 * Mnemonics: Wn UDIV (n=1..4)
 * Operands: 3 (<a/r/t>, <b/r/t>, <c/w/t>)
 *
 * Opcodes:
 *   0xFE48-0xFE4B (W1 UDIV through W4 UDIV) - Word unsigned divide
 *
 * Operation: <a> / <b> -> <c>, remainder -> Rn
 *
 * Description:
 *   The operands are treated as unsigned. The <a> operand is divided
 *   by the <b> operand, and the quotient is stored in <c>. The
 *   remainder is stored in the specified register. Byte and halfword
 *   constants are sign-extended and treated as unsigned.
 *
 * Flags: Z (zero), S (sign), DZ (divide by zero)
 *   Z = 1 if quotient is zero
 *   S = 1 if quotient sign bit is set
 *   DZ = 1 if divisor is zero
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Divide by zero (DZ)
 *
 * Reference: ND-500 Reference Manual, Chapter 11.16
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Udiv.cs
 */
void nd500_instr_Udiv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 47-51) */
    if (fi->operand_count != 3) {
        printf("[ERROR] UDIV at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands as unsigned (like C# lines 54-55) */
    /* UDIV is always word-sized regardless of fi->data_type */
    uint32_t dividend = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    uint32_t divisor = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Check for divide by zero (like C# lines 58-63) */
    if (divisor == 0) {
        ND500X_TRAPLOG("[TRAP] UDIV at PC=0x%08X: Divide by zero\n", fi->address);
        trap_divide_by_zero(cpu, fi->address);
        return;
    }

    /* Perform unsigned division (like C# lines 68-69) */
    uint32_t quotient = dividend / divisor;
    uint32_t remainder = dividend % divisor;

    /* Write quotient to destination (like C# line 72) */
    nd500_write_operand_value(cpu, &fi->operands[2], (uint64_t)quotient, ND500_DTYPE_WORD);

    /* Write remainder to register (like C# line 75) */
    nd500_write_integer_register(cpu, fi->target_register, remainder);

    /* Update status flags (like C# lines 78-79) */
    /* Set Z flag based on quotient */
    if (quotient == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    /* Set S flag based on quotient sign bit (even though unsigned, bit 31 still matters) */
    if ((quotient & 0x80000000) != 0) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }

    /* Clear carry and overflow flags - unsigned division doesn't set these */
    cpu->ST1 &= ~(ND500_FLAG_C | ND500_FLAG_O);
}
