#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SCOMP instruction - COMPARE class
 *
 * Mnemonic: scomp
 * Operands: 2
 * Opcode: 0xFDAC (175254 octal)
 *
 * Operation: Compare string (unpacked) elements
 *
 * Description:
 * Compares two strings element by element, using I1 as index into the first
 * string and I2 as index into the second string. Comparison continues until
 * a difference is found or the end of either string is reached.
 *
 * Both operands specify string descriptors pointing to string data in memory.
 * Each descriptor is an 8-byte structure containing:
 *   - Byte address of string data
 *   - Element count (number of elements)
 *   - Element size (byte, word, double-word)
 *   - Format flags
 *
 * String Comparison Algorithm:
 * 1. Load string descriptor 1 from first operand address
 * 2. Load string descriptor 2 from second operand address
 * 3. Initialize index1 = I1, index2 = I2
 * 4. While index1 < length1 AND index2 < length2:
 *    a. Read element from string1 at index1
 *    b. Read element from string2 at index2
 *    c. If elements differ:
 *       - Set flags based on comparison
 *       - Exit loop
 *    d. Increment index1 and index2
 * 5. If no difference found:
 *    - Compare string lengths to determine result
 * 6. Update I1 = index1, I2 = index2
 *
 * Flag Behavior:
 * - Z (Zero): Set if strings are equal (all elements match, same length)
 * - S (Sign): Set if string1 < string2 lexicographically
 * - C (Carry): Set if string1 > string2 lexicographically
 * - K (Invalid): Unaffected
 * - O (Overflow): Unaffected
 *
 * Comparison Results:
 *   string1 == string2: Z=1, S=0, C=0
 *   string1 <  string2: Z=0, S=1, C=0
 *   string1 >  string2: Z=0, S=0, C=1
 *
 * Lexicographic Comparison:
 * - Compares elements byte by byte (unsigned comparison)
 * - First difference determines the result
 * - If all elements match but lengths differ:
 *   - Shorter string is considered "less than" longer string
 *   - Example: "ABC" < "ABCD"
 *
 * Index Registers:
 * - I1: Starting index into first string (input), final index (output)
 * - I2: Starting index into second string (input), final index (output)
 * - Final indices point to:
 *   - Position where difference was found, OR
 *   - End of string if no difference found
 *
 * String Element Format:
 * - Typically byte elements (ASCII/EBCDIC characters)
 * - Can be word or double-word elements per descriptor
 * - Element size specified in descriptor format field
 *
 * Trap Conditions:
 * - Descriptor Range (DR): If I1 or I2 exceeds string bounds
 * - Invalid descriptor format
 * - Memory access violations during element reads
 *
 * Memory Access Pattern:
 * 1. Read 8 bytes from operand[0] address (descriptor 1)
 * 2. Read 8 bytes from operand[1] address (descriptor 2)
 * 3. Read elements from descriptor 1's data address
 * 4. Read elements from descriptor 2's data address
 * 5. Number of reads depends on where first difference occurs
 *
 * Performance:
 * - Execution time depends on string lengths and position of first difference
 * - Best case: First elements differ (10-15 CPU cycles)
 * - Worst case: Long equal strings (cycles ≈ 10 + 5×min(length1, length2))
 * - Early exit on first difference improves average case
 *
 * Typical Usage:
 *   ; Compare two file names
 *   CLR   I1              ; Start at beginning
 *   CLR   I2
 *   SCOMP DESC_NAME1, DESC_NAME2
 *   JZ    NAMES_EQUAL    ; Branch if identical
 *   JS    NAME1_LESS     ; Branch if name1 < name2
 *   ; Fall through: name1 > name2
 *
 *   ; Search for string in array
 * LOOP:
 *   CLR   I2              ; Search string index = 0
 *   SCOMP DESC_ARRAY, DESC_SEARCH
 *   JZ    FOUND           ; Match found
 *   ; Increment array pointer and continue
 *
 * Notes:
 * - Comparison is unsigned (bytes 0-255, not signed -128 to +127)
 * - Index registers are updated regardless of comparison result
 * - Useful for string sorting, searching, and equality testing
 * - Different from PCOMP which compares BCD numeric values
 * - Strings do not need to be null-terminated (length from descriptor)
 * - Comparison stops at end of either string (not necessarily null terminator)
 * - Case-sensitive comparison (A != a)
 *
 * Comparison with PCOMP:
 * - PCOMP: Numeric comparison of packed BCD values
 * - SCOMP: Lexicographic comparison of string elements
 * - PCOMP: Sets Z and S flags only
 * - SCOMP: Sets Z, S, and C flags
 * - PCOMP: Does not use index registers
 * - SCOMP: Uses and updates I1 and I2
 *
 * Related Instructions:
 * - PCOMP: Compare packed BCD strings
 * - SMOVE: String move operation
 * - SSCAN: String scan operation
 * - SSPAN: String span operation
 *
 * Reference: ND-500 Reference Manual ND-05.009.4 EN, Page 240, Section 13.13
 */
void nd500_instr_Scomp(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Get descriptor addresses from operands */
    uint32_t desc_addr1 = fi->operands[0].effective_address;
    uint32_t desc_addr2 = fi->operands[1].effective_address;

    /* Load string descriptor 1 from first operand address */
    Nd500StringDescriptor desc1;
    if (!nd500_load_string_descriptor(cpu, desc_addr1, false, true, &desc1)) {
        /* Invalid descriptor - K flag already set by helper */
        return;
    }

    /* Load string descriptor 2 from second operand address */
    Nd500StringDescriptor desc2;
    if (!nd500_load_string_descriptor(cpu, desc_addr2, false, true, &desc2)) {
        /* Invalid descriptor - K flag already set by helper */
        return;
    }

    /* Get starting indices from I1 and I2 registers */
    uint32_t index1 = cpu->I[0];  /* I1 */
    uint32_t index2 = cpu->I[1];  /* I2 */

    /* Compare strings element by element */
    bool strings_equal = true;
    bool string1_less = false;

    /* Compare while both indices are within bounds */
    while (index1 < desc1.element_count && index2 < desc2.element_count) {
        /* Read element from string 1 */
        /* For string comparison, we assume byte elements (typical for text strings) */
        uint32_t addr1 = desc1.base_address + index1;
        uint8_t element1 = nd500_bus_read8(cpu->machine, addr1);

        /* Read element from string 2 */
        uint32_t addr2 = desc2.base_address + index2;
        uint8_t element2 = nd500_bus_read8(cpu->machine, addr2);

        /* Check if elements differ */
        if (element1 != element2) {
            /* Found difference - determine which is less */
            strings_equal = false;
            string1_less = (element1 < element2);
            break;  /* Exit comparison loop */
        }

        /* Elements match - continue to next */
        index1++;
        index2++;
    }

    /* If no difference found in compared elements, check lengths */
    if (strings_equal) {
        /* Check if one string is longer than the other */
        if (index1 < desc1.element_count) {
            /* String 1 is longer (has more elements) - string1 > string2 */
            strings_equal = false;
            string1_less = false;
        } else if (index2 < desc2.element_count) {
            /* String 2 is longer (has more elements) - string1 < string2 */
            strings_equal = false;
            string1_less = true;
        }
        /* Else: both at end, strings are equal */
    }

    /* Update index registers with final positions */
    cpu->I[0] = index1;  /* I1 */
    cpu->I[1] = index2;  /* I2 */

    /* Set status flags based on comparison result */
    /* Z = 1 if strings are equal */
    if (strings_equal) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    /* S = 1 if string1 < string2 */
    if (string1_less) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* C = 1 if string1 > string2 */
    if (!strings_equal && !string1_less) {
        nd500_set_flag(cpu, ND500_FLAG_C);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_C);
    }

    /* K and O flags are unaffected */

    /* PC will be advanced automatically by cpu_step() */
}
