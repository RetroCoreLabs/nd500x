#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Temm2Get instruction - MOVE class
 *
 * Store TEMM2 Register: TEMM2 → <dest>
 *
 * Variants: 1
 * Mnemonics: temm2=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFE53
 *
 * Operation: regs.TEMM2 → <dest>
 *
 * Description:
 *   Reads the TEMM2 (Table Entry Memory Management 2) register and stores it
 *   to the destination operand as a word (32-bit). Updates Z and S flags based
 *   on the value.
 *
 *   The TEMM registers are used for table entry memory management operations,
 *   providing support for virtual memory and segmentation.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if TEMM2 is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Temm2Get.cs
 */
void nd500_instr_Temm2Get(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] Temm2Get at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = cpu->TEMM2;
    nd500_write_operand_word(cpu, &fi->operands[0], value);

    // Set Z and S flags based on the value
    if (value == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    if ((value & 0x80000000) != 0) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }
}
