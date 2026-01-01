#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * BMOVE instruction - Block Move or Fill
 *
 * Format: t BMOVE <source>, <dest>, <count>
 *
 * Opcodes:
 *   BY BMOVE  0xFD20  - byte block move
 *   H  BMOVE  0xFE78  - halfword block move
 *   W  BMOVE  0xFE79  - word block move
 *   F  BMOVE  0xFE7A  - float block move
 *   D  BMOVE  0xFE7B  - double float block move
 *
 * Two Operating Modes:
 *   1. Block Copy (source is memory): copy count elements from source to dest
 *   2. Block Fill (source is register/constant): fill dest with count copies of source value
 *
 * Operands:
 *   [0] source - source address OR fill value (read, type t)
 *   [1] dest   - destination address (write, type t)
 *   [2] count  - number of elements (read, always Word)
 *
 * Trap conditions: Addressing traps
 * Data status bits: All cleared (Z, S, C, V -> 0)
 *
 * Reference: ND-500 Reference Manual Section 15.1
 *            ~/repos/nd500x/docs/instructions/asm/bmove.md
 */
void nd500_instr_Bmove(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 3) {
        printf("[ERROR] BMOVE expects 3 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        return;
    }

    /* Read count (operand 2, always Word) */
    uint32_t count = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[2], ND500_DTYPE_WORD);

    /* Get element size from data type */
    uint32_t element_size;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:      element_size = 1; break;
        case ND500_DTYPE_HALFWORD:  element_size = 2; break;
        case ND500_DTYPE_WORD:      element_size = 4; break;
        case ND500_DTYPE_DOUBLEWORD: element_size = 8; break;
        default:
            printf("[ERROR] BMOVE: Invalid data type %u at PC=0x%08X\n",
                   fi->data_type, fi->address);
            return;
    }

    /* Check if source is register/constant (fill mode) or memory (copy mode) */
    bool fill_mode = (fi->operands[0].mode == ND500_ADDR_CONSTANT ||
                      fi->operands[0].mode == ND500_ADDR_CONSTANT_SHORT ||
                      fi->operands[0].mode == ND500_ADDR_REGISTER);

    uint64_t fill_value = 0;
    uint32_t src_base = 0;

    if (fill_mode) {
        /* Fill mode: source is the value to fill with */
        fill_value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    } else {
        /* Copy mode: source is memory address */
        src_base = fi->operands[0].effective_address;
    }

    uint32_t dst_base = fi->operands[1].effective_address;

    /* Determine copy direction for overlapping regions.
     * If dst > src and regions overlap, copy backward to avoid corruption.
     * This is the classic memmove() vs memcpy() problem.
     */
    uint32_t total_bytes = count * element_size;
    bool copy_backward = false;

    if (!fill_mode) {
        /* Check for overlap: dst is within [src, src + total_bytes) */
        if (dst_base > src_base && dst_base < src_base + total_bytes) {
            copy_backward = true;
        }
    }

    /* Transfer elements */
    for (uint32_t iter = 0; iter < count; iter++) {
        /* Calculate index based on copy direction */
        uint32_t i = copy_backward ? (count - 1 - iter) : iter;

        uint32_t dst_addr = dst_base + (i * element_size);
        uint64_t value;

        if (fill_mode) {
            value = fill_value;
        } else {
            uint32_t src_addr = src_base + (i * element_size);
            switch (fi->data_type) {
                case ND500_DTYPE_BYTE:
                    value = nd500_read_memory_8(cpu, src_addr);
                    break;
                case ND500_DTYPE_HALFWORD:
                    value = nd500_read_memory_16(cpu, src_addr);
                    break;
                case ND500_DTYPE_WORD:
                    value = nd500_read_memory_32(cpu, src_addr);
                    break;
                case ND500_DTYPE_DOUBLEWORD:
                    value = nd500_read_memory_64(cpu, src_addr);
                    break;
                default:
                    value = 0;
                    break;
            }

            /* Check for trap after read (e.g., MMU page fault) */
            if (cpu->machine && !cpu->machine->run_flag) {
                break;  /* Trap occurred - stop iteration */
            }
        }

        /* Write to destination */
        switch (fi->data_type) {
            case ND500_DTYPE_BYTE:
                nd500_write_memory_8(cpu, dst_addr, (uint8_t)value);
                break;
            case ND500_DTYPE_HALFWORD:
                nd500_write_memory_16(cpu, dst_addr, (uint16_t)value);
                break;
            case ND500_DTYPE_WORD:
                nd500_write_memory_32(cpu, dst_addr, (uint32_t)value);
                break;
            case ND500_DTYPE_DOUBLEWORD:
                nd500_write_memory_64(cpu, dst_addr, value);
                break;
        }

        /* Check for trap after write (e.g., MMU page fault, protection violation) */
        if (cpu->machine && !cpu->machine->run_flag) {
            break;  /* Trap occurred - stop iteration */
        }
    }

    /* All flags cleared per ND-500 documentation */
    nd500_clear_flag(cpu, ND500_FLAG_Z);
    nd500_clear_flag(cpu, ND500_FLAG_S);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
