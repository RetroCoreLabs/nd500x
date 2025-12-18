#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Svers instruction - SYSTEM class
 *
 * SVERS - Store Version
 *
 * Format: SVERS <destination/w/W>
 *
 * Assembly:
 *   SVERS (store version)                    Hex 0xFFFB
 *
 * Operation: <VERSION> -> <destination>
 *
 * Description:
 *   Store version number in destination address. The version number
 *   identifies the specific implementation of the ND-500 CPU architecture.
 *   This instruction is used by system software to determine which
 *   features and capabilities are available.
 *
 * Trap conditions: Addressing traps
 *
 * Data status bits: Z (zero if version is 0), S (sign bit of version)
 *
 * Reference: ND-500 Reference Manual, Chapter 16.35
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Svers.cs
 */
void nd500_instr_Svers(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 41-45) */
    if (fi->operand_count != 1) {
        printf("[ERROR] SVERS at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Define version number - implementation specific (like C# line 49) */
    /* Version 1.0.0.0 - same as C# implementation */
    const uint32_t VERSION_NUMBER = 0x00010000;

    /* Write version number to destination operand (like C# line 52) */
    nd500_write_operand_value(cpu, &fi->operands[0], VERSION_NUMBER, ND500_DTYPE_WORD);

    /* Set status flags based on version (like C# lines 57-59) */
    /* Z = 1 if version is 0 */
    if (VERSION_NUMBER == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    /* S = sign bit of version */
    if (VERSION_NUMBER & 0x80000000) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }
}
