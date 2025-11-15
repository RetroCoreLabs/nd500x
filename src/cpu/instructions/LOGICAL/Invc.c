#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Invc instruction - LOGICAL class
 *
 * Invert register with carry (one's complement + carry).
 * Rn = ~Rn + C
 *
 * Variants: 4 (by register)
 * Mnemonics: W1 INVC, W2 INVC, W3 INVC, W4 INVC
 * Operands: 0 (register-only operation)
 *
 * Opcodes:
 *   0xFF10-0xFF13 (W1 INVC through W4 INVC) - Word invert with carry
 *
 * Operation: Rn ← ~Rn + C (one's complement + carry)
 *
 * Description:
 *   The one's complement of the contents of the specified register is
 *   calculated, the carry flag is added, and the result is stored in
 *   the same register. The carry flag is updated if the addition
 *   produces a carry out. This instruction is useful for implementing
 *   two's complement negation and multi-word arithmetic.
 *
 * Flags: Z (zero), S (sign), C (carry)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if addition produced carry
 *
 * Trap conditions: None
 *
 * Reference: ND-500 Reference Manual, Chapter 10.14
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/LOGICAL/Invc.cs
 */
void nd500_instr_Invc(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 0) {
        printf("[ERROR] INVC at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read register value (like C# line 19) */
    uint32_t value = nd500_read_integer_register(cpu, fi->target_register);

    /* Read carry flag (like C# line 20) */
    uint32_t carry = (cpu->FLAGS & ND500_FLAG_C) ? 1 : 0;

    /* One's complement + carry (like C# line 21) */
    uint64_t result64 = ((uint64_t)~value) + carry;

    /* Get lower 32 bits (like C# line 22) */
    uint32_t result = (uint32_t)result64;

    /* Detect carry out (like C# line 23) */
    bool new_carry = (result64 > 0xFFFFFFFF);

    /* Write back to register (like C# line 24) */
    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Update status flags: Z, S, C (like C# line 25) */
    /* Note: C# always uses DataType.W for INVC */
    nd500_set_flags_zsc(cpu, result, ND500_DTYPE_WORD, new_carry);
}
