#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * SCHPAR instruction - STRING class
 *
 * SCHPAR - Check parity in string (ND-500 Reference Manual Section 14.20)
 *
 * Format: BY SCHPAR <string/r/BY/I1=>, <mode/r/BY>
 *
 * Assembly:
 *   BY SCHPAR (check parity in string)  Hex 0xFDB5
 *
 * Operation:
 *   0 -> Z
 *   while not end of string
 *       and bit 7 of S(I1) = parity according to <mode> do
 *           I1 + 1 -> I1
 *       enddo
 *   if bit 7 of S(I1) != parity according to <mode> then
 *       1 -> Z
 *   endif
 *
 * Mode values:
 *   0 - clear parity (bit 7 should be 0)
 *   1 - set parity (bit 7 should be 1)
 *   2 - even parity (bit 7 should make total 1-bits even)
 *   3 - odd parity (bit 7 should make total 1-bits odd)
 *
 * Terminating conditions:
 *   - outside string: K=0 Z=0 I1 unmodified, DR trap condition
 *   - string empty: K=0 Z=0 I1 -> next element
 *   - parity error found: K=0 Z=1 I1 -> element with wrong parity
 *
 * Reference: ND-500 Reference Manual, Chapter 14.20
 */
void nd500_instr_Schpar(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] SCHPAR at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* First operand: string descriptor address
     * Can be ABSOLUTE (effective_address is the address) or
     * CONSTANT (value is the descriptor address, effective_address is 0)
     */
    uint32_t string_desc_addr;
    if (fi->operands[0].mode == ND500_ADDR_CONSTANT ||
        fi->operands[0].mode == ND500_ADDR_CONSTANT_SHORT) {
        /* For constant addressing, the descriptor address IS the constant value */
        string_desc_addr = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    } else {
        string_desc_addr = fi->operands[0].effective_address;
    }

    /* Second operand: mode value (0-3) */
    uint8_t mode = nd500_read_operand_byte(cpu, &fi->operands[1]);

    /* Validate mode */
    if (mode > 3) {
        printf("[ERROR] SCHPAR at PC=0x%08X: Invalid mode %u (must be 0-3)\n",
               fi->address, mode);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Load string descriptor */
    Nd500StringDescriptor string_desc;
    if (!nd500_load_string_descriptor(cpu, string_desc_addr, false, true, &string_desc)) {
        return;
    }

    /* Get starting index from I1 */
    uint32_t index = cpu->I[0];

    /* Clear Z flag initially */
    nd500_clear_flag(cpu, ND500_FLAG_Z);

    /* Check if starting index is outside string */
    if (index >= string_desc.element_count) {
        /* Outside string - K=0, Z=0, I1 unmodified */
        nd500_clear_flag(cpu, ND500_FLAG_K);
        nd500_set_flag(cpu, ND500_FLAG_S);  /* Signal outside string */
        return;
    }

    /* Helper to check if parity is correct for a byte */
    bool parity_ok;
    while (index < string_desc.element_count) {
        uint32_t addr = string_desc.base_address + index;
        uint8_t byte_val = nd500_bus_read8(cpu->machine, addr);
        uint8_t bit7 = (byte_val >> 7) & 1;

        switch (mode) {
            case 0:  /* Clear parity - bit 7 should be 0 */
                parity_ok = (bit7 == 0);
                break;
            case 1:  /* Set parity - bit 7 should be 1 */
                parity_ok = (bit7 == 1);
                break;
            case 2:  /* Even parity - count of 1-bits should be even */
            case 3:  /* Odd parity - count of 1-bits should be odd */
            {
                /* Count 1-bits in lower 7 bits */
                int count = 0;
                for (int i = 0; i < 7; i++) {
                    if (byte_val & (1 << i)) count++;
                }
                /* For even parity: bit 7 should make total even */
                /* For odd parity: bit 7 should make total odd */
                int total_ones = count + bit7;
                bool is_even = (total_ones % 2) == 0;
                parity_ok = (mode == 2) ? is_even : !is_even;
                break;
            }
            default:
                parity_ok = true;  /* Should not reach here */
                break;
        }

        if (!parity_ok) {
            /* Parity error found - set Z, I1 points to bad element */
            nd500_set_flag(cpu, ND500_FLAG_Z);
            cpu->I[0] = index;
            nd500_clear_flag(cpu, ND500_FLAG_K);
            nd500_clear_flag(cpu, ND500_FLAG_S);
            return;
        }

        index++;
    }

    /* All bytes passed parity check */
    cpu->I[0] = index;  /* I1 points past last element */
    nd500_clear_flag(cpu, ND500_FLAG_K);
    nd500_clear_flag(cpu, ND500_FLAG_Z);  /* No error */
    nd500_clear_flag(cpu, ND500_FLAG_S);
}
