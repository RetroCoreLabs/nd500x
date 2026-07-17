#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Sloca instruction - SYSTEM class (STRING operation)
 *
 * SLOCA - String Locate Element
 *
 * Format: t SLOCA <source/r/t/I1=>, <test/r/BI,BY>
 *
 * Assembly:
 *   BI SLOCA (string locate bit)             Hex 0xFDAF
 *   BY SLOCA (string locate byte)            Hex 0xFDB0
 *
 * Operation:
 *   while not end of string and S(I1) <> <test> do
 *     I1 + 1 -> I1
 *   enddo
 *   if S(I1) >> <test> then 0 -> S else 1 -> S endif
 *
 * Description:
 *   Elements are skipped in the <source> string until an element
 *   equal to the <test> operand is found or the end of the string
 *   is reached. The S bit is set to 1 if the end of the string
 *   is reached, otherwise it is set to 0.
 *
 *   EMULATOR NOTE: String descriptor operations require string descriptor
 *   infrastructure. This is a basic implementation that treats the source
 *   as a simple memory buffer.
 *
 * Trap conditions: Addressing traps, Descriptor range (DR)
 *
 * Terminating conditions:
 *   - outside source: K=0 Z=0 I1 unmodified, DR trap condition
 *   - matching element: K=0 Z=1 I1 := matching element
 *   - source empty: K=0 Z=0 I1 := next element
 *
 * Reference: ND-500 Reference Manual, Chapter 14.15
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Sloca.cs
 */
void nd500_instr_Sloca(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 54-58) */
    if (fi->operand_count != 2) {
        printf("[ERROR] SLOCA at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Operand 0 IS the string descriptor, in place - its effective address is the
     * descriptor address, exactly as Smove/Sfill take it. This routine used to
     * read a VALUE from operand 0 (dereferencing the descriptor's first word) and
     * treat that as the descriptor address, which is wrong: with the descriptor
     * {count=7, base=0xB0001D3C} sitting in frame slot b.0x14, it took the count 7
     * as the descriptor address and read garbage. Operand 1 is the test value. */
    uint32_t desc_address = fi->operands[0].effective_address;
    uint32_t test_value = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Load string descriptor from memory.
     *
     * The field order is [element_count(32)] [base_address(32)] - word0 is the
     * COUNT, word1 is the BASE. This routine had it BACKWARDS (base first), so it
     * searched from the count value as if it were an address: with a real
     * descriptor {count=7, base=0xB0001D3C} it read base=7 and took a protect
     * violation at vaddr=0x00000007. Every other STRING instruction goes through
     * nd500_load_string_descriptor(); use it here too rather than re-deriving the
     * layout. It also gives the correct element size per data type instead of a
     * hard-coded 1. */
    Nd500StringDescriptor desc;
    if (!nd500_load_string_descriptor(cpu, desc_address, false, true, &desc)) {
        return;
    }
    uint32_t base_address = desc.base_address;
    uint32_t element_count = desc.element_count;

    /* String operations use I1 as index register (like C# line 68) */
    uint32_t index = cpu->I[0];

    /* Check for empty string (like C# lines 71-79) */
    if (element_count == 0) {
        cpu->ST1 &= ~ND500_FLAG_K;
        cpu->ST1 &= ~ND500_FLAG_Z;
        cpu->ST1 |= ND500_FLAG_S;  /* End of string reached */
        return;
    }

    /* Check if starting index is outside source (like C# lines 82-91) */
    if (index >= element_count) {
        cpu->ST1 &= ~ND500_FLAG_K;
        cpu->ST1 &= ~ND500_FLAG_Z;
        cpu->ST1 |= ND500_FLAG_S;
        /* DR trap condition - set in ST2 */
        return;
    }

    /* Search for matching element (like C# lines 94-108) */
    bool found = false;
    while (index < element_count) {
        /* Read element from the string at the descriptor's data type (BI/BY/H/W),
         * honouring bit addressing for BI. */
        uint64_t element = nd500_string_read_element(cpu, &desc, index, fi->data_type);

        if ((uint32_t)element == test_value) {
            /* Found matching element */
            found = true;
            break;
        }

        index++;
    }

    /* Update I1 register (like C# line 111) */
    cpu->I[0] = index;

    /* Set status flags based on result (like C# lines 114-127) */
    if (found) {
        /* Matching element: K=0 Z=1 I1 := matching element */
        cpu->ST1 &= ~ND500_FLAG_K;
        cpu->ST1 |= ND500_FLAG_Z;
        cpu->ST1 &= ~ND500_FLAG_S;  /* Match found (not end of string) */
    } else {
        /* End of string reached without match: S=1 */
        cpu->ST1 &= ~ND500_FLAG_K;
        cpu->ST1 &= ~ND500_FLAG_Z;
        cpu->ST1 |= ND500_FLAG_S;   /* End of string reached */
    }
}
