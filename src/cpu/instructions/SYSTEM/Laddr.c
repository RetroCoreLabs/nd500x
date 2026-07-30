#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Laddr instruction - SYSTEM class
 *
 * LADDR - Load Address
 *
 * Format: tn LADDR <operand/aa/t>
 *
 * Variants: 6 (by data type and register)
 * Mnemonics: BIn LADDR, BYn LADDR, Hn LADDR, Wn LADDR, Fn LADDR, Dn LADDR (n=1..4)
 * Operands: 1 (<operand/aa/t>)
 *
 * Opcodes:
 *   0xFE20-0xFE23 (BI1-BI4 LADDR) - Bit load address
 *   0xFE24-0xFE27 (BY1-BY4 LADDR) - Byte load address
 *   0xFE28-0xFE2B (H1-H4 LADDR)   - Halfword load address
 *   0xFD3C-0xFD3F (W1-W4 LADDR)   - Word load address
 *   0xFD3C-0xFD3F (F1-F4 LADDR)   - Float load address (same as W)
 *   0xFE2C-0xFE2F (D1-D4 LADDR)   - Double load address
 *
 * Operation: addr(<operand>) -> Rn
 *
 * Description:
 *   The address of the operand is loaded into the specified register.
 *   Registers and constants have no address in memory and are illegal
 *   as operands. Formats other than Wn are used to give the correct
 *   scaling factor if <operand> is indexed. Fn is equivalent to Wn,
 *   but may improve readability.
 *
 * Flags: Z (zero)
 *   Z = 1 if loaded address is 0
 *   S, C, O = 0 (cleared)
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Illegal operand if operand is register or constant
 *
 * Reference: ND-500 Reference Manual, Chapter 15.4
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Laddr.cs
 */
void nd500_instr_Laddr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 47-51) */
    if (fi->operand_count != 1) {
        printf("[ERROR] LADDR at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get operand */
    const Nd500OperandDecoded* op = &fi->operands[0];

    /* Check for illegal operands: registers have no address */
    if (op->mode == ND500_ADDR_REGISTER) {
        printf("[ERROR] LADDR at PC=0x%08X: Cannot take address of register\n",
               fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get effective address from operand (like C# line 54) */
    uint32_t address;

    /* For CONSTANT and CONSTANT_SHORT modes, the "address" is the constant value itself.
     * For memory addressing modes, use the computed effective_address */
    if (op->mode == ND500_ADDR_CONSTANT_SHORT) {
        /* Value embedded in address code (0x00-0x3F) */
        address = op->address_code & 0x3F;
    } else if (op->mode == ND500_ADDR_CONSTANT) {
        /* Value in op->data (big-endian) */
        switch (op->data_len) {
            case 1: address = op->data[0]; break;
            case 2: address = (op->data[0] << 8) | op->data[1]; break;
            case 4: address = (op->data[0] << 24) | (op->data[1] << 16) |
                              (op->data[2] << 8) | op->data[3]; break;
            default: address = 0; break;
        }
    } else {
        /* Memory addressing mode - use computed effective address */
        address = op->effective_address;
    }

    /* Load address into target register (like C# line 57) */
    nd500_write_integer_register(cpu, fi->target_register, address);

    /* Update status flags (like C# lines 60-63) */
    /* Z = 1 if address is 0 */
    if (address == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    /* S from the RESOLVED-ADDRESS sign via the microcode ST,SAVA. ADJUDICATED against the real
     * B30 microcode LADDRN @000762 ("ALU,A A,DAC,EAO ... ST,SAVA"): the address is passed through
     * the ALU and all four data-status bits are latched, so S = address bit31 (NOT a forced 0).
     * The manual lists only "address==0 -> Z". C,O = 0. Matches RetroCore Laddr.cs. */
    cpu->ST1 &= ~(ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O);
    if (address & 0x80000000u) cpu->ST1 |= ND500_FLAG_S;
}
