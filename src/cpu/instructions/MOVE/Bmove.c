#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * BMOVE instruction - MOVE class
 *
 * Mnemonic: bmove (with type prefix: BY, H, W, D)
 * Operands: 3
 * Opcodes: 0xFD20, 0xFE78, 0xFE79, 0xFE7A, 0xFE7B (176040, 177170, 177171, 177172, 177173 octal)
 *
 * Operation: Block move - Transfer multiple elements from source to destination
 *
 * Description:
 * Transfers a block of data elements from a source memory region to a destination
 * memory region. The number of elements to transfer is specified in the third operand.
 * Element size is determined by the data type prefix (BY=byte, H=halfword, W=word, D=doubleword).
 *
 * Operand Structure:
 * - Operand[0]: Source address (read) - base address of source block
 * - Operand[1]: Destination address (write) - base address of destination block
 * - Operand[2]: Count (read, word) - number of elements to transfer
 *
 * Data Type Variants:
 * - BY BMOVE: Transfer bytes (8-bit elements)
 * - H BMOVE: Transfer halfwords (16-bit elements)
 * - W BMOVE: Transfer words (32-bit elements)
 * - D BMOVE: Transfer doublewords (64-bit elements)
 *
 * Operation Steps:
 * 1. Read count value from operand[2] (always a word, 32-bit)
 * 2. For i = 0 to count-1:
 *    a. Calculate source address: source_base + (i × element_size)
 *    b. Calculate destination address: dest_base + (i × element_size)
 *    c. Read element from source address
 *    d. Write element to destination address
 * 3. Update status flags based on last element transferred
 *
 * Block Transfer Example (W BMOVE):
 *   Source:      0x1000: [0xAABBCCDD, 0x11223344, 0x55667788]
 *   Destination: 0x2000: [?, ?, ?]
 *   Count:       3
 *
 *   After execution:
 *   Destination: 0x2000: [0xAABBCCDD, 0x11223344, 0x55667788]
 *
 * Memory Access Pattern:
 * 1. Read count from operand[2] address (4 bytes, word)
 * 2. For each element (0..count-1):
 *    - Read element_size bytes from source[i]
 *    - Write element_size bytes to dest[i]
 * Total memory accesses: 1 read (count) + count reads (elements) + count writes (elements)
 *
 * Flag Behavior:
 * - Z (Zero): Set if last transferred element is zero, cleared otherwise
 * - S (Sign): Set if last transferred element has sign bit set, cleared otherwise
 * - C (Carry): Unaffected
 * - K (Invalid): Unaffected
 * - O (Overflow): Unaffected
 *
 * Special Cases:
 * - If count == 0: No transfer occurs, flags unchanged
 * - If count == 1: Single element transfer, flags based on that element
 * - Overlapping regions: Behavior is undefined if source and destination overlap
 *   (forward copy may corrupt data if dest > source && dest < source + count × size)
 *
 * Trap Conditions:
 * - Addressing traps if source or destination address is invalid
 * - Memory access violations if regions are not accessible
 * - Alignment traps may occur depending on system requirements
 *
 * Performance:
 * - Execution time: O(count) - linear in number of elements
 * - Typical: 5-8 CPU cycles per element (depends on element size)
 * - Large transfers (count > 1000) may be optimized by hardware DMA
 *
 * Typical Usage:
 *   ; Copy 10 words from buffer1 to buffer2
 *   W BMOVE  BUFFER1, BUFFER2, #10
 *
 *   ; Initialize array with zeros (after clearing first element)
 *   CLR      ARRAY
 *   W BMOVE  ARRAY, ARRAY+4, #99   ; Copy zero to remaining 99 elements
 *
 *   ; Copy string (byte array)
 *   BY BMOVE SOURCE_STR, DEST_STR, STRING_LEN
 *
 *   ; Copy double-precision float array
 *   D BMOVE  SRC_DOUBLES, DST_DOUBLES, ARRAY_SIZE
 *
 * Notes:
 * - This is a memory-to-memory operation (no register involvement except addressing)
 * - Transfer proceeds in ascending address order (element 0, 1, 2, ...)
 * - For overlapping regions where dest < source, use reverse copy (not provided)
 * - Count is always read as a word (32-bit), regardless of element type
 * - Status flags reflect only the LAST element transferred, not all elements
 * - Useful for buffer copies, array initialization, and data structure duplication
 * - More efficient than loop of individual load/store instructions
 *
 * Comparison with Other Instructions:
 * - BMOVE: Block transfer with explicit count (this instruction)
 * - SMOVE: String move using string descriptors (different addressing)
 * - MOV: Single element move (count=1 implicit)
 *
 * Memory Overlap Scenarios:
 *
 * Safe (no overlap):
 *   Source: [====]
 *   Dest:           [====]
 *
 * Safe (dest before source):
 *   Source:     [====]
 *   Dest:   [====]
 *   (forward copy is safe)
 *
 * UNSAFE (dest within source, forward copy corrupts):
 *   Source: [========]
 *   Dest:       [========]
 *   (would need reverse copy, not supported by BMOVE)
 *
 * Related Instructions:
 * - SMOVE: String move using descriptors
 * - SMVTR, SMVTU, SMVWH, SMVUN: String move variants
 * - SSCAN, SSPAN: String scanning operations
 *
 * Reference: ND-500 Reference Manual, String/Block Operations
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Bmove.cs
 */
void nd500_instr_Bmove(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Verify operand count */
    if (fi->operand_count != 3) {
        printf("[ERROR] BMOVE expects 3 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        return;
    }

    /* Get source and destination base addresses from operands */
    uint32_t source_addr = fi->operands[0].effective_address;
    uint32_t dest_addr = fi->operands[1].effective_address;

    /* Read count from third operand (always a word/32-bit value) */
    uint32_t count = nd500_read_memory_32(cpu, fi->operands[2].effective_address);

    /* Determine element size from data type */
    uint32_t element_size;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            element_size = 1;  /* BY = 8-bit */
            break;
        case ND500_DTYPE_HALFWORD:
            element_size = 2;  /* H = 16-bit */
            break;
        case ND500_DTYPE_WORD:
            element_size = 4;  /* W = 32-bit */
            break;
        case ND500_DTYPE_DOUBLEWORD:
            element_size = 8;  /* D = 64-bit */
            break;
        default:
            printf("[ERROR] BMOVE: Unknown data type %u at PC=0x%08X\n",
                   fi->data_type, fi->address);
            return;
    }

    /* Transfer elements one by one */
    uint64_t last_value = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Calculate addresses with index offset */
        uint32_t src_addr = source_addr + (i * element_size);
        uint32_t dst_addr = dest_addr + (i * element_size);

        /* Read element from source and write to destination */
        switch (fi->data_type) {
            case ND500_DTYPE_BYTE: {
                uint8_t value = nd500_read_memory_8(cpu, src_addr);
                nd500_write_memory_8(cpu, dst_addr, value);
                last_value = value;
                break;
            }
            case ND500_DTYPE_HALFWORD: {
                uint16_t value = nd500_read_memory_16(cpu, src_addr);
                nd500_write_memory_16(cpu, dst_addr, value);
                last_value = value;
                break;
            }
            case ND500_DTYPE_WORD: {
                uint32_t value = nd500_read_memory_32(cpu, src_addr);
                nd500_write_memory_32(cpu, dst_addr, value);
                last_value = value;
                break;
            }
            case ND500_DTYPE_DOUBLEWORD: {
                uint64_t value = nd500_read_memory_64(cpu, src_addr);
                nd500_write_memory_64(cpu, dst_addr, value);
                last_value = value;
                break;
            }
        }
    }

    /* Update status flags based on last element transferred (if any) */
    if (count > 0) {
        /* Z = 1 if last value is zero */
        if (last_value == 0) {
            nd500_set_flag(cpu, ND500_FLAG_Z);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_Z);
        }

        /* S = 1 if last value has sign bit set (depends on element size) */
        bool sign_bit_set = false;
        switch (fi->data_type) {
            case ND500_DTYPE_BYTE:
                sign_bit_set = (last_value & 0x80) != 0;
                break;
            case ND500_DTYPE_HALFWORD:
                sign_bit_set = (last_value & 0x8000) != 0;
                break;
            case ND500_DTYPE_WORD:
                sign_bit_set = (last_value & 0x80000000) != 0;
                break;
            case ND500_DTYPE_DOUBLEWORD:
                sign_bit_set = (last_value & 0x8000000000000000ULL) != 0;
                break;
        }

        if (sign_bit_set) {
            nd500_set_flag(cpu, ND500_FLAG_S);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_S);
        }
    }

    /* C, K, O flags are unaffected */

    /* PC will be advanced automatically by cpu_step() */
}
