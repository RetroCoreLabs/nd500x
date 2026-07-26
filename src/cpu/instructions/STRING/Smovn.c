#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SMOVN instruction - STRING class
 *
 * SMOVN - String move n elements
 *
 * Format: t SMOVN <source/r/t/I1=>, <dest/w/t/I2=>, <count/r/W>
 *
 * Assembly:
 *   BI SMOVN (string move n bits)        Hex 0xFD76
 *   BY SMOVN (string move n bytes)       Hex 0xFD77
 *   H  SMOVN (string move n halfwords)   Hex 0xFD78
 *   W  SMOVN (string move n words)       Hex 0xFD79
 *   F  SMOVN (string move n floats)      Hex 0xFD7A
 *   D  SMOVN (string move n doubles)     Hex 0xFD7B
 *
 * Operation:
 *   for i = 1 to <count> do
 *     S(I1) -> D(I2)
 *     I1 + 1 -> I1, I2 + 1 -> I2
 *   enddo
 *
 * Description:
 *   Exactly <count> elements are moved from <source> to <dest>.
 *
 * Reference: ND-500 Reference Manual, Chapter 14.3
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Smovn.cs
 */
void nd500_instr_Smovn(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 3) {
        printf("[ERROR] SMOVN at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses and count */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t dest_desc_addr = fi->operands[1].effective_address;
    uint32_t count = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[2], ND500_DTYPE_WORD);

    /* Load string descriptors */
    Nd500StringDescriptor source_desc, dest_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, false, true, &source_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, false, true, &dest_desc)) {
        return;
    }

    /* Get starting indices from I1 and I2 */
    uint32_t src_index = cpu->I[0];
    uint32_t dest_index = cpu->I[1];

    /* Move exactly count elements */
    for (uint32_t i = 0; i < count; i++) {
        if (src_index >= source_desc.element_count ||
            dest_index >= dest_desc.element_count) {
            break;
        }

        uint32_t src_addr = source_desc.base_address + src_index;
        uint8_t value = nd500_read_memory_8(cpu, src_addr);

        uint32_t dest_addr = dest_desc.base_address + dest_index;
        nd500_write_memory_8(cpu, dest_addr, value);

        src_index++;
        dest_index++;
    }

    /* Update index registers */
    cpu->I[0] = src_index;
    cpu->I[1] = dest_index;

    /* Set status flags */
    /* Z is left 0 on completion. The real B30 microcode terminator is ALU,A A,BM00 B,X1 ST,SAVA
       (A,BM00 = 1<<0 = 1) -> Z=0; the termination REASON is carried in K (SOUR_RANGE @003117 K,ZRO ->
       source exhausted K=0, DEST_RANGE @003122 K,ONE -> dest full K=1), never re-latching Z. The green
       SFILL follows this; setting Z=1 diverged from the microword on every completed move. */
    nd500_clear_flag(cpu, ND500_FLAG_Z);
    if (dest_index >= dest_desc.element_count) {
        nd500_set_flag(cpu, ND500_FLAG_K);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_K);
    }
    nd500_clear_flag(cpu, ND500_FLAG_S);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
