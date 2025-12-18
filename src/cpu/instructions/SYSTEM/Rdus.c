#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Rdus instruction - SYSTEM class
 *
 * RDUS - Read (Load) Bypassing Cache (Uncached Segment)
 *
 * Format: tn RDUS <source/r/t>
 *
 * Assembly:
 *   BIn RDUS (load bit, bypass cache)        Hex 0xFEA0+(n-1)
 *   BYn RDUS (load byte, bypass cache)       Hex 0xFEA4+(n-1)
 *   Hn  RDUS (load halfword, bypass cache)   Hex 0xFEA8+(n-1)
 *   Wn  RDUS (load word, bypass cache)       Hex 0xFEAC+(n-1)
 *
 * Operation: <source> -> Rn
 *
 * Description:
 *   The operand is loaded from main memory, disregarding cache contents.
 *   This is primarily useful after a DMA transfer to memory has been
 *   performed to prevent use of obsolete data in the cache. Register and
 *   constant operands are illegal and will cause an illegal operand
 *   specifier trap condition.
 *
 *   EMULATOR NOTE: In the emulator without a cache, this performs a
 *   regular read from memory. The semantics are identical to a normal
 *   load instruction since all reads go directly to memory.
 *
 * Trap conditions: Addressing traps, Illegal operand specifier (IOS)
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.25
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Rdus.cs
 */
void nd500_instr_Rdus(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 45-49) */
    if (fi->operand_count != 1) {
        printf("[ERROR] RDUS at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate target register */
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] RDUS at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    const Nd500OperandDecoded* op = &fi->operands[0];

    /* Check for illegal operands: registers and constants have no memory address */
    /* Reference: "Register and constant operands are illegal" */
    if (op->mode == ND500_ADDR_REGISTER ||
        op->mode == ND500_ADDR_CONSTANT ||
        op->mode == ND500_ADDR_CONSTANT_SHORT) {
        printf("[ERROR] RDUS at PC=0x%08X: Cannot use register or constant operand\n",
               fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Load operand from main memory bypassing cache (like C# lines 60-61) */
    /* In emulator without cache, this is identical to normal load */
    uint64_t value = nd500_read_operand_value(cpu, op, fi->data_type);

    /* Store loaded value into target register */
    nd500_write_integer_register(cpu, fi->target_register, (uint32_t)value);

    /* No status bits affected for this instruction */
}
