#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Temm1Get instruction - MOVE class
 *
 * Store TEMM1 Register: TEMM1 -> <dest>
 *
 * Variants: 1
 * Mnemonics: temm1=:
 * Operands: 1 (<dest/w/t>)
 *
 * Opcode: 0xFE52
 *
 * Operation: regs.TEMM1 -> <dest>
 *
 * Description:
 *   Reads the TEMM1 (Table Entry Memory Management 1) register and stores it
 *   to the destination operand as a word (32-bit). Updates Z and S flags based
 *   on the value.
 *
 *   The TEMM registers are used for table entry memory management operations,
 *   providing support for virtual memory and segmentation.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if TEMM1 is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Temm1Get.cs
 */
void nd500_instr_Temm1Get(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] Temm1Get at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = cpu->TEMM1;
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
