#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Rladdr instruction - SYSTEM class
 *
 * RLADDR - Load Address into Record Register
 *
 * Format: t RLADDR <operand/aa/t>
 *
 * Assembly:
 *   BI RLADDR (bit load address to R)        Hex 0xFC55
 *   BY RLADDR (byte load address to R)       Hex 0xFC5A
 *   H  RLADDR (halfword load address to R)   Hex 0xFCB1
 *   W  RLADDR (word load address to R)       Hex 0xBE
 *   F  RLADDR (float load address to R)      Hex 0xBE
 *   D  RLADDR (double float load address to R) Hex 0xFCB2
 *
 * Operation: addr(<operand>) -> R
 *
 * Description:
 *   The address of the operand is loaded into the record register.
 *   Registers and constants have no address in memory and are illegal
 *   as operands. The record register R is used for indirect addressing
 *   and record manipulation operations.
 *
 * Trap conditions: Addressing traps
 *
 * Data status bits: Z (address = 0), S/C/O = 0
 *
 * Reference: ND-500 Reference Manual, Chapter 15.5
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Rladdr.cs
 */
void nd500_instr_Rladdr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 46-49) */
    if (fi->operand_count != 1) {
        printf("[ERROR] RLADDR at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    const Nd500OperandDecoded* op = &fi->operands[0];

    /* Check for illegal operands: registers and constants have no memory address */
    if (op->mode == ND500_ADDR_REGISTER) {
        printf("[ERROR] RLADDR at PC=0x%08X: Cannot take address of register\n",
               fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get operand address (like C# line 53) */
    uint32_t operand_address = op->effective_address;

    /* Load calculated address into record register R (like C# line 56) */
    cpu->R = operand_address;

    /* Set Z flag if address is zero, clear S/C/O (like C# lines 59-62) */
    if (operand_address == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }
    /* S from the RESOLVED-ADDRESS sign via the microcode ST,SAVA (RLADDR @000767). Matches
     * RetroCore Rladdr.cs; the manual lists only "address==0 -> Z". C,O = 0. */
    cpu->ST1 &= ~(ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O);
    if (operand_address & 0x80000000u) cpu->ST1 |= ND500_FLAG_S;
}
