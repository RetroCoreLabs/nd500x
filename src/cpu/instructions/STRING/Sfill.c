#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * SFILL instruction - STRING class
 *
 * SFILL - String fill
 *
 * Format: tn SFILL <dest/w/t/I2=>
 *
 * Assembly:
 *   BIn SFILL (bit string fill)           Hex 0xFD7C+(n-1)
 *   BYn SFILL (byte string fill)          Hex 0xFD80+(n-1)
 *   Hn  SFILL (halfword string fill)      Hex 0xFD84+(n-1)
 *   Wn  SFILL (word string fill)          Hex 0xFD88+(n-1)
 *   Fn  SFILL (float string fill)         Hex 0xFD8C+(n-1)
 *   Dn  SFILL (double float string fill)  Hex 0xFD90+(n-1)
 *
 * Operation:
 *   while not end of string do
 *     tn -> D(I2)
 *     I2 + 1 -> I2
 *   enddo
 *
 * Description:
 *   The contents of the specified register are put into every element
 *   of the <dest> string starting at the element specified by I2.
 *
 * Terminating conditions:
 *   - outside dest: K=1 I2 unmodified, DR trap condition
 *   - string filled: K=1 I2 := next element
 *
 * Reference: ND-500 Reference Manual, Chapter 14.8
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Sfill.cs
 */
void nd500_instr_Sfill(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] SFILL at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor address */
    uint32_t dest_desc_addr = fi->operands[0].effective_address;

    /* Load string descriptor */
    Nd500StringDescriptor dest_desc;
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, false, true, &dest_desc)) {
        return;
    }

    /* Get fill value from target register */
    uint32_t fill_value;
    if (fi->target_register >= 1 && fi->target_register <= 4) {
        fill_value = nd500_read_integer_register(cpu, fi->target_register);
    } else {
        fill_value = 0;
    }

    /* Get starting index from I2 */
    uint32_t dest_index = cpu->I[1];

    /* Fill elements using proper data type handling */
    while (dest_index < dest_desc.element_count) {
        nd500_string_write_element(cpu, &dest_desc, dest_index, fill_value, fi->data_type);
        dest_index++;
    }

    /* Update I2 register */
    cpu->I[1] = dest_index;

    /* Set K flag - destination full */
    nd500_set_flag(cpu, ND500_FLAG_K);
    nd500_clear_flag(cpu, ND500_FLAG_Z);
    nd500_string_clear_unused_flags(cpu);  /* Clears S, C, O */
}
