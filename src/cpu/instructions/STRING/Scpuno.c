#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SCPUNO instruction - STRING class (opcode block), but NOT a string loop.
 *
 * SCPUNO - Store CPU number ('87 extension)
 *
 * Format: SCPUNO <destination/w>          (Manual 16.36)
 * Opcode: 0xFF7CC / 177774B  (dispatch entry uses 0xFFFC, operand_count = 1)
 *
 * Operation: <CPUNO> -> <destination>
 *
 * Description:
 *   Store the CPU number in the destination address. This emulator models a
 *   single ND-500/ND-5000 CPU, so the CPU number is 0 (same convention as the
 *   RetroCore C# emulator: "Returns 0 (single CPU)").
 *
 * Data Status Bits:
 *   Manual 16.36: "Status bit set according to CPU number." The microcode
 *   (SCPUNO_1 @011021 -> SAVE_RES @011025) writes the arithmetic status
 *   (ST,SAVA) from the stored CPU-number value. For value 0 that is Z=1, S=0;
 *   C and O are not named for this instruction and are cleared (rule 4040).
 *
 * Reference: ND-500 Reference Manual, Section 16.36.
 *            Microcode SCPUNO 001047 / SCPUNO_1 011021 (SAMSON_CPU, SAVE_RES 011025).
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Scpuno.cs
 */
void nd500_instr_Scpuno(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] SCPUNO at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Single-CPU emulation: CPU number is 0 */
    uint32_t cpuno = 0;

    /* <CPUNO> -> <destination> (destination is a WORD write operand) */
    nd500_write_operand_value(cpu, &fi->operands[0], cpuno, ND500_DTYPE_WORD);

    /* Status per the stored CPU-number value (ST,SAVA): Z if zero, S if sign
     * bit set. C and O not named for SCPUNO -> cleared (rule 4040). */
    nd500_set_flags_zs(cpu, cpuno, ND500_DTYPE_WORD);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
