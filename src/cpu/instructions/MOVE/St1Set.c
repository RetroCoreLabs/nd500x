#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * St1Set instruction - MOVE class
 *
 * Load ST1 Register: <source> → ST1
 *
 * Variants: 1
 * Mnemonics: st1:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFDB9
 *
 * Operation: <source> → regs.ST.ST1
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and loads it MASKED (AND 0o7773777740)
 *   into the ST1 status word, PRESERVING the live condition flags Z/C/S/O (no ST,SAVA in the
 *   LOAST1/LOAD_ST1 microcode path - the flags are left unchanged, exactly like st1=:).
 *
 *   The ST1 register contains CPU status flags including Z (zero), S (sign),
 *   C (carry), and O (overflow) flags. This instruction allows programs to
 *   restore a previously saved processor status.
 *
 * Flags: NONE changed - st1:= leaves the condition flags unchanged (no ST,SAVA, per B30)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/St1Set.cs
 */
void nd500_instr_St1Set(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] St1Set at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // st1:= LOADS the ST1 status word (masked) but does NOT latch the live macro condition
    // flags Z/C/S/O - they are left UNCHANGED, exactly like the store st1=:.
    //
    // Verified against the REAL B30 microcode: the whole LOAST1 path
    //   LOAST1 @001012 (READ ADACT -> SC5), @001013 (save old status -> SC6),
    //   LOAD_ST1 @012052 : SC5 = value AND 0o7773777740 (= 0x3FEFFFE0, mask off protected bits
    //     0-4 Reserved0/PIA/PD/IR/PSD and bit17 SIT),
    //   @012053..@012070 : merge SC5 with the old status (SC6) + IDU status, committing via
    //     ST,LOAD @012062 (internal AluSts) and D,IDU,STS.
    // There is NO ST,SAVA / ST,SAVC anywhere in that path (the next instruction LOATE1 @001014
    // has ST,SAVA - st1:= deliberately does not), so the macro-visible Z/C/S/O are untouched.
    // The old code recomputed Z/S from value-as-a-number, which was wrong twice over.
    uint32_t value = nd500_read_operand_word(cpu, &fi->operands[0]);
    const uint32_t COND_FLAGS = ND500_FLAG_Z | ND500_FLAG_C | ND500_FLAG_S | ND500_FLAG_O;
    uint32_t old_flags = cpu->ST1 & COND_FLAGS;                     // preserve the live condition flags
    cpu->ST1 = ((value & 0x3FEFFFE0u) & ~COND_FLAGS) | old_flags;   // load masked word, keep Z/C/S/O
}
