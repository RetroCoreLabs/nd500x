#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SCOPT instruction - STRING class
 *
 * SCOPT - "String COmpare Translated with PAd" (Manual 14.13)
 *
 * Format: BY SCOPT <source-1/r/BY/I1=>, <source-2/r/BY/I2=>,
 *                  <trans table/aa/BY>, <pad/r/BY>
 * Opcode: 0xFDBF / 176677B
 * Microcode: 001323 SCOPT -> 007022 SCOPTBY_F01 (mismatch SCOPTBY_MIS 007114).
 *
 * SCOPT is SCOPA (compare-with-pad) with every compared byte - including the pad
 * substituted past a string's end - passed through a 256-byte translate table.
 * It is NOT a "copy-translate-until-stop" (that was a different, wrong
 * instruction; the real op writes nothing and compares).
 *
 * Operation (manual 14.13):
 *   tpad = table[pad]
 *   compare, substituting the (translated) pad past a string's end; an index is
 *   only advanced while it is still inside its own string:
 *       a = table[ (I1<L1)? S(I1) : pad ]
 *       b = table[ (I2<L2)? D(I2) : pad ]
 *       d = a - b ; if d != 0 break ; else I1+=1, I2+=1
 *
 * Terminating conditions (manual 14.13, same K/Z/S as SCOPA):
 *   | condition                      | K | Z | S | I1,I2                 |
 *   | exact match (incl. pad)        | 0 | 1 | 0 | :- next element       |
 *   | greater byte in source-1       | 1 | 0 | 0 | :- differing elements |
 *   | smaller byte in source-1       | 1 | 0 | 1 | :- differing elements |
 *   C and O are cleared for all string operations.
 *
 * Operands: <source-1/r/BY/I1>, <source-2/r/BY/I2>, <trans table/aa/BY>, <pad/r/BY>.
 *
 * Traps: DR trap (operand outside its string); ILL_OP_SPEC (bad table, 007027).
 *
 * Reference: ND-500 Reference Manual, Section 14.13.
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Scopt.cs
 */
void nd500_instr_Scopt(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 4) {
        printf("[ERROR] SCOPT at PC=0x%08X: Expected 4 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* operand[0]/[1] string descriptors; operand[2] translate-table base
     * (256-byte, absolute address); operand[3] pad BYTE VALUE. */
    Nd500StringDescriptor desc1, desc2;
    if (!nd500_load_string_descriptor(cpu, fi->operands[0].effective_address, false, true, &desc1)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, fi->operands[1].effective_address, false, true, &desc2)) {
        return;
    }
    uint32_t table_addr = fi->operands[2].effective_address;
    uint8_t pad = (uint8_t)nd500_read_operand_value(cpu, &fi->operands[3], ND500_DTYPE_BYTE);

    uint32_t index1 = cpu->I[0];  /* I1 */
    uint32_t index2 = cpu->I[1];  /* I2 */

    bool strings_equal = true;
    bool string1_less = false;

    /* Compare until BOTH strings are exhausted; an exhausted string yields pad.
     * Every compared byte (including pad) is translated through the table. */
    while (index1 < desc1.element_count || index2 < desc2.element_count) {
        uint8_t raw1 = (index1 < desc1.element_count)
            ? nd500_read_memory_8(cpu, desc1.base_address + index1) : pad;
        uint8_t raw2 = (index2 < desc2.element_count)
            ? nd500_read_memory_8(cpu, desc2.base_address + index2) : pad;

        uint8_t a = nd500_read_memory_8(cpu, table_addr + raw1);
        uint8_t b = nd500_read_memory_8(cpu, table_addr + raw2);

        if (a != b) {
            strings_equal = false;
            string1_less = (a < b);
            break;
        }

        /* Advance each index only while still inside its own string. */
        if (index1 < desc1.element_count) index1++;
        if (index2 < desc2.element_count) index2++;
    }

    cpu->I[0] = index1;  /* I1 */
    cpu->I[1] = index2;  /* I2 */

    /* Flags: same convention as SCOPA (source1 < source2 -> S=1). */
    if (strings_equal) {
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_set_flag(cpu, ND500_FLAG_Z);
        nd500_clear_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_set_flag(cpu, ND500_FLAG_K);
        nd500_clear_flag(cpu, ND500_FLAG_Z);
        if (string1_less) {
            nd500_set_flag(cpu, ND500_FLAG_S);   /* source1 < source2 -> S=1 */
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_S); /* source1 > source2 -> S=0 */
        }
    }

    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
