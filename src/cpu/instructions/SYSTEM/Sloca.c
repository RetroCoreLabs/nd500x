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

    /* Read descriptor address and test value (like C# lines 61-62) */
    uint32_t desc_address = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    uint32_t test_value = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Load string descriptor from memory */
    /* Descriptor format: [base_address(32)] [element_count(32)] */
    uint32_t base_address = nd500_read_memory_32(cpu, desc_address);
    uint32_t element_count = nd500_read_memory_32(cpu, desc_address + 4);

    /* String operations use I1 as index register (like C# line 68) */
    uint32_t index = cpu->I[0];

    /* Determine element size - both bit and byte operations use 1 byte in memory */
    uint32_t element_size = 1;

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
        /* Read element from string */
        uint32_t element_address = base_address + (index * element_size);
        uint32_t element;

        /* Read byte element */
        element = nd500_read_memory_8(cpu, element_address);

        if (element == test_value) {
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
