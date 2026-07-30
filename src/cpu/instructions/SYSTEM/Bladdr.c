#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Bladdr instruction - SYSTEM class
 *
 * BLADDR - Load Address into Base Register
 *
 * Format: t BLADDR <operand/aa/t>
 *
 * Assembly:
 *   BI BLADDR (bit load address to B)        Hex 0xFCB3
 *   BY BLADDR (byte load address to B)       Hex 0xFCBC
 *   H  BLADDR (halfword load address to B)   Hex 0xFD37
 *   W  BLADDR (word load address to B)       Hex 0xFD63
 *   F  BLADDR (float load address to B)      Hex 0xFD63
 *   D  BLADDR (double float load address to B) Hex 0xFD38
 *
 * Operation: addr(<operand>) -> B
 *
 * Description:
 *   The address of the operand is loaded into the base register.
 *   Registers and constants have no address in memory and are illegal
 *   as operands. The base register B is used for base-relative addressing
 *   and provides a base address for memory access operations.
 *
 * Trap conditions: Addressing traps
 *
 * Data status bits: Z (address = 0), S/C/O = 0
 *
 * Reference: ND-500 Reference Manual, Chapter 15.6
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Bladdr.cs
 */
void nd500_instr_Bladdr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 46-49) */
    if (fi->operand_count != 1) {
        printf("[ERROR] BLADDR at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    const Nd500OperandDecoded* op = &fi->operands[0];

    /* Check for illegal operands: registers and constants have no memory address */
    if (op->mode == ND500_ADDR_REGISTER) {
        printf("[ERROR] BLADDR at PC=0x%08X: Cannot take address of register\n",
               fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get operand address (like C# line 53) */
    uint32_t operand_address = op->effective_address;

    /* Load calculated address into base register B (like C# line 56) */
    cpu->B = operand_address;

    /* Set Z flag if address is zero, clear S/C/O (like C# lines 59-62) */
    if (operand_address == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }
    /* S from the RESOLVED-ADDRESS sign via the microcode ST,SAVA (BLADDR @000772,
     * "ALU,A A,DAC,EAO ... ST,SAVA"). Matches RetroCore Bladdr.cs; the manual lists only
     * "address==0 -> Z". C,O = 0. */
    cpu->ST1 &= ~(ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O);
    if (operand_address & 0x80000000u) cpu->ST1 |= ND500_FLAG_S;
}
