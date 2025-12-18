#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Wdus instruction - SYSTEM class
 *
 * WDUS - Write (Store) Bypassing Cache (Uncached Segment)
 *
 * Format: tn WDUS <dest/w/t>
 *
 * Assembly:
 *   BIn WDUS (store bit, bypass cache)       Hex 0xFEB0+(n-1)
 *   BYn WDUS (store byte, bypass cache)      Hex 0xFEB4+(n-1)
 *   Hn  WDUS (store halfword, bypass cache)  Hex 0xFEB8+(n-1)
 *   Wn  WDUS (store word, bypass cache)      Hex 0xFEBC+(n-1)
 *
 * Operation: Rn -> <dest>
 *
 * Description:
 *   The operand is stored to main memory, disregarding cache contents.
 *   This is primarily useful before a DMA transfer from memory to prevent
 *   stale cache data from being used. Register and constant operands are
 *   illegal and will cause an illegal operand specifier trap condition.
 *
 *   EMULATOR NOTE: In the emulator without a cache, this performs a
 *   regular write to memory. The semantics are identical to a normal
 *   store instruction since all writes go directly to memory.
 *
 * Trap conditions: Addressing traps, Illegal operand specifier (IOS)
 *
 * Data status bits: None affected
 *
 * Reference: ND-500 Reference Manual, Chapter 16.25
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Wdus.cs
 */
void nd500_instr_Wdus(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 45-49) */
    if (fi->operand_count != 1) {
        printf("[ERROR] WDUS at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate target register */
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] WDUS at PC=0x%08X: Invalid target register %u\n",
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
        printf("[ERROR] WDUS at PC=0x%08X: Cannot use register or constant operand\n",
               fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read value from target register (like C# line 52) */
    uint32_t value = nd500_read_integer_register(cpu, fi->target_register);

    /* Write to destination, bypassing cache (like C# line 56) */
    /* In emulator with no cache, this is identical to normal write */
    nd500_write_operand_value(cpu, op, value, fi->data_type);

    /* No status bits affected for this instruction */
}
