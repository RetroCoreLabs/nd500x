#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Umul instruction - ARITHMETIC class
 *
 * Unsigned Multiply with Overflow to Register: <a> * <b> → <c>, overflow → Rn
 *
 * Variants: 1 (word-only)
 * Mnemonics: Wn UMUL (n=1..4)
 * Operands: 3 (<a/r/t>, <b/r/t>, <c/w/t>)
 *
 * Opcodes:
 *   0xFC80-0xFC83 (W1 UMUL through W4 UMUL) - Word unsigned multiply
 *
 * Operation: <a> * <b> → <c>, overflow part → Rn
 *
 * Description:
 *   The operands are treated as unsigned. The <a> operand is multiplied
 *   by the <b> operand, and the product is stored in <c>. The upper half
 *   of the double-length result is stored in the specified register. Byte
 *   and halfword constants are sign-extended and treated as unsigned.
 *   Integer overflow occurs when the upper part differs from zero.
 *
 * Flags: Z (zero), S (sign), O (overflow)
 *   Z = 1 if product (lower 32 bits) is zero
 *   S = 1 if product sign bit is set
 *   O = 1 if overflow (upper 32 bits non-zero)
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *
 * Reference: ND-500 Reference Manual, Chapter 11.15
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Umul.cs
 */
void nd500_instr_Umul(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 48-52) */
    if (fi->operand_count != 3) {
        printf("[ERROR] UMUL at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands as unsigned (like C# lines 55-56) */
    /* UMUL is always word-sized regardless of fi->data_type */
    uint32_t a = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    uint32_t b = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Perform unsigned multiplication (64-bit result) (like C# line 59) */
    uint64_t product = (uint64_t)a * (uint64_t)b;

    /* Lower 32 bits go to destination (like C# line 62) */
    uint32_t lowerHalf = (uint32_t)(product & 0xFFFFFFFF);

    /* Upper 32 bits go to register (like C# line 65) */
    uint32_t upperHalf = (uint32_t)(product >> 32);

    /* Write results (like C# lines 68-69) */
    nd500_write_operand_value(cpu, &fi->operands[2], (uint64_t)lowerHalf, ND500_DTYPE_WORD);
    nd500_write_integer_register(cpu, fi->target_register, upperHalf);

    /* Update status flags (like C# lines 72-74) */
    /* Set Z flag based on lower half */
    if (lowerHalf == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    /* Set S flag based on lower half sign bit */
    if ((lowerHalf & 0x80000000) != 0) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }

    /* Set O flag based on upper half (overflow if upper half is non-zero) */
    bool overflow = (upperHalf != 0);
    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    /* Clear carry flag - unsigned multiplication doesn't set carry */
    cpu->ST1 &= ~ND500_FLAG_C;

    /* TEST: do NOT trap on UMUL overflow. UMUL is a WIDENING unsigned multiply -
     * the low 32 bits go to <c> and the HIGH 32 bits are delivered to register Rn
     * BY DESIGN (line 64). A non-zero upper half is the intended result, not an
     * error. The ND-500 overflow trap is ignorable (gated by the domain's Own Trap
     * Enable); the NDIX kernel does ordinary unsigned multiplies (e.g. nelem*size)
     * expecting the high half in Rn and NO trap. Unconditionally trapping here
     * raised a spurious invalid-operation trap that aborted the init-creation path,
     * leaving the run queue empty and idling forever at text 0x844. Same class as
     * the BYCONV overflow-trap fix. Just leave the O flag set (above) and continue. */
    if (overflow && getenv("ND500X_UMULDBG")) {
        printf("[UMUL] PC=0x%08X: overflow (upper=0x%08X delivered to Rn) - O flag set, no trap\n",
               fi->address, upperHalf);
    }
}
