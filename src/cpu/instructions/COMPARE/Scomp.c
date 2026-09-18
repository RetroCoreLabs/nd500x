/*
 * Scomp.c - ND-500 SCOMP instruction (STRING class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SCOMP instruction - STRING class
 *
 * Mnemonic: scomp
 * Operands: 2
 * Opcode: 0xFDAC (176654 octal)
 *
 * Format: BY SCOMP (<source-1/r/BY/I1>=,<source-2/r/BY/I2>=)
 *
 * Operation: Compare string (unpacked) elements
 *
 * Description:
 * Bytes from the <source-1> string are compared with the corresponding bytes
 * in the <source-2> string until unequal bytes are found, or until the end of
 * <source-1> or <source-2> string is reached. When unequal bytes are found,
 * the status bits Z and S and the K flag will indicate the termination condition.
 * The byte elements are considered to be unsigned values.
 *
 * String Comparison Algorithm:
 * 1. Load string descriptor 1 from first operand address
 * 2. Load string descriptor 2 from second operand address
 * 3. Initialize index1 = I1, index2 = I2
 * 4. While index1 < length1 AND index2 < length2:
 *    a. Read element from string1 at index1
 *    b. Read element from string2 at index2
 *    c. If elements differ:
 *       - Set K=1, S based on byte comparison
 *       - Exit loop
 *    d. Increment index1 and index2
 * 5. If no difference found:
 *    - Set K=0, S based on length comparison
 * 6. Update I1 = index1, I2 = index2
 *
 * Terminating conditions (per ND-500 Reference Manual Page 240):
 *
 * | Condition                      | K | Z | S | I1, I2                    |
 * |--------------------------------|---|---|---|---------------------------|
 * | both operands outside string   | 0 | 1 | 0 | unmodified, DR trap       |
 * | exact match                    | 0 | 1 | 0 | next element              |
 * | source-1 longer                | 0 | 0 | 0 | next element              |
 * | source-2 longer                | 0 | 0 | 1 | next element              |
 * | smaller byte in source-1       | 1 | 0 | 0 | differing elements        |
 * | greater byte in source-1       | 1 | 0 | 1 | differing elements        |
 *
 * Data status bits (per ND-500 Reference Manual Page 240, 244):
 *   K = 1 if byte difference found, 0 if length mismatch
 *   Z = 1 if strings are equal
 *   S meaning depends on K:
 *     - K=0, S=0: source-1 longer (source-1 > source-2)
 *     - K=0, S=1: source-2 longer (source-1 < source-2)
 *     - K=1, S=0: smaller byte in source-1 (source-1 < source-2)
 *     - K=1, S=1: greater byte in source-1 (source-1 > source-2)
 *   C = 0 (cleared for all string operations)
 *   O = 0 (cleared for all string operations)
 *
 * Index Registers:
 * - I1: Starting index into first string (input), final index (output)
 * - I2: Starting index into second string (input), final index (output)
 * - Final indices point to:
 *   - Position where difference was found, OR
 *   - End of string if no difference found
 *
 * Trap Conditions:
 * - Descriptor Range (DR): If both operands are outside string bounds
 *
 * Notes:
 * - Comparison is unsigned (bytes 0-255, not signed -128 to +127)
 * - Index registers are updated regardless of comparison result
 * - Strings do not need to be null-terminated (length from descriptor)
 * - Case-sensitive comparison (A != a)
 *
 * Related Instructions:
 * - SCOTR: String compare translated
 * - SCOPA: String compare with pad
 * - SCOPT: String compare translated with pad
 *
 * Reference: ND-500 Reference Manual ND-05.009.4 EN, Page 240, Section 14.10
 */
void nd500_instr_Scomp(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Get descriptor addresses from operands */
    uint32_t desc_addr1 = fi->operands[0].effective_address;
    uint32_t desc_addr2 = fi->operands[1].effective_address;

    /* Load string descriptor 1 from first operand address */
    Nd500StringDescriptor desc1;
    if (!nd500_load_string_descriptor(cpu, desc_addr1, &desc1)) {
        /* Invalid descriptor - K flag already set by helper */
        return;
    }

    /* Load string descriptor 2 from second operand address */
    Nd500StringDescriptor desc2;
    if (!nd500_load_string_descriptor(cpu, desc_addr2, &desc2)) {
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
        /* For string comparison, we assume byte elements (typical for text strings).
         * String descriptor base_address is a VIRTUAL address - it must go through
         * the MMU (nd500_read_memory_8), not a raw physical bus read, or with the
         * MMU enabled we compare the wrong bytes. */
        uint32_t addr1 = desc1.base_address + index1;
        uint8_t element1 = nd500_read_memory_8(cpu, addr1);

        /* Read element from string 2 */
        uint32_t addr2 = desc2.base_address + index2;
        uint8_t element2 = nd500_read_memory_8(cpu, addr2);

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

    /* Set status flags per ND-500 Reference Manual Page 240, 244 */

    /* Determine if termination was due to byte difference or length mismatch */
    /* K = 1 if byte difference found (loop exited due to element mismatch) */
    /* K = 0 if length mismatch (loop exited because one string ended) */
    bool byte_diff_found = !strings_equal &&
        (index1 < desc1.element_count && index2 < desc2.element_count);

    if (byte_diff_found) {
        /* Byte difference found: K=1. Per ND-500 Ref Manual sect 14.10 (p254):
         * smaller byte in source-1 (source1 < source2) -> S=1;
         * greater byte in source-1 (source1 > source2) -> S=0.
         * This matches the COMP sign convention (S=1 <=> source1 < source2) that the
         * shared conditional branches (if>=go tests S=0, if<go tests S=1) rely on.
         * The previous code had this inverted, breaking NC's keyword binary search. */
        nd500_set_flag(cpu, ND500_FLAG_K);
        if (string1_less) {
            nd500_set_flag(cpu, ND500_FLAG_S);   /* source1 < source2 -> S=1 */
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_S); /* source1 > source2 -> S=0 */
        }
    } else {
        /* Length mismatch or equal strings: K=0 */
        nd500_clear_flag(cpu, ND500_FLAG_K);
        /* S = 1 if source-2 longer (source-1 < source-2) */
        /* S = 0 if source-1 longer or equal (source-1 >= source-2) */
        if (!strings_equal && index2 < desc2.element_count) {
            nd500_set_flag(cpu, ND500_FLAG_S);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_S);
        }
    }

    /* Z = 1 if strings are equal */
    if (strings_equal) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    /* C and O flags are ALWAYS CLEARED for string operations (ND-500 Manual page 244) */
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);

    /* PC will be advanced automatically by cpu_step() */
}
