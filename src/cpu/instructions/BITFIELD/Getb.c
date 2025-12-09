#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * GETB instruction - BITFIELD class
 *
 * Mnemonic: getb (with register prefix: W1, W2, W3, W4)
 * Operands: 1
 * Opcode: 0xFE4C (177114 octal)
 *
 * Operation: Get Block from Buddy System Heap - Allocate memory block and return address
 *
 * Description:
 * Allocates a data block from the buddy system heap. The size is specified as a
 * logarithmic value (log_size), where the actual block size is 2^log_size words.
 * The address of the allocated block is loaded into the target register (I1-I4 or A1-A4).
 *
 * The ND-500 buddy system is a memory allocation algorithm that maintains free lists
 * for blocks of power-of-2 sizes. This allows fast allocation and efficient memory
 * utilization by splitting and coalescing blocks.
 *
 * Register Variants:
 * - W1 GETB <log_size>: Allocate block, address → I1
 * - W2 GETB <log_size>: Allocate block, address → I2
 * - W3 GETB <log_size>: Allocate block, address → I3
 * - W4 GETB <log_size>: Allocate block, address → I4
 *
 * Operand Structure:
 * - Operand[0]: log_size (read, byte) - logarithmic block size (0-31)
 *
 * Block Size Calculation:
 * - block_size = 2^log_size words
 * - Example: log_size=3 → block_size = 2^3 = 8 words = 32 bytes
 * - Example: log_size=10 → block_size = 2^10 = 1024 words = 4096 bytes
 *
 * Operation Steps:
 * 1. Read log_size from operand (byte value, 0-31)
 * 2. Calculate block_size = 2^log_size words
 * 3. Check if block_size > MAXL (maximum block size from heap variables)
 * 4. Search freelists for available block of requested size:
 *    a. Check freelist[log_size] for block of exact size
 *    b. If empty, check larger freelists[log_size+1, log_size+2, ...]
 *    c. If found larger block, split it recursively until desired size
 * 5. If no blocks available, raise Stack Overflow (STO) trap
 * 6. Unlink allocated block from freelist
 * 7. Write block address to target register
 * 8. Update heap administration counters
 *
 * Heap Variables (pointed to by TOS register):
 * The TOS register points to a heap variable structure containing:
 * - Offset +0: MAXL (word, 4 bytes) - Maximum logarithmic size (e.g., 15 for 32K word blocks)
 * - Offset +4: STAH (word, 4 bytes) - Start of heap address (unused by GETB, for trap handlers)
 * - Offset +8: ENDH (word, 4 bytes) - End of heap address (unused by GETB, for trap handlers)
 * - Offset +12: FLOG[0] - Freelist head for 2^0 word blocks (word, 4 bytes)
 * - Offset +16: FLOG[1] - Freelist head for 2^1 word blocks (word, 4 bytes)
 * - Offset +20: FLOG[2] - Freelist head for 2^2 word blocks (word, 4 bytes)
 * - ...
 * - Offset +12+(k*4): FLOG[k] - Freelist head for 2^k word blocks
 *   Each freelist entry is a word containing address of first free block
 * - Free blocks are linked lists: first word of block = address of next free block
 *
 * This structure matches ND-500 Reference Manual §3.3 and §15.13-15.14.
 * STAH and ENDH are documented but not used by heap instructions (available for trap handlers).
 *
 * Buddy System Algorithm:
 *
 * Allocation:
 * 1. If freelist[k] has blocks, unlink and return
 * 2. Otherwise, find freelist[m] where m > k with available block
 * 3. Split block from freelist[m]:
 *    - Remove block from freelist[m]
 *    - Split in half, add buddy to freelist[m-1]
 *    - Repeat splitting until reaching freelist[k]
 * 4. Return allocated block from freelist[k]
 *
 * Block Structure:
 * Free blocks store link to next free block in first word:
 *   Word[0]: Address of next free block (0 if end of list)
 *   Word[1..size-1]: Unused (available for allocation)
 *
 * Flag Behavior:
 * - Z (Zero): Set if allocated address is 0 (should not happen except on error)
 * - S (Sign): Set if allocated address has sign bit set
 * - C (Carry): Unaffected
 * - K (Invalid): Unaffected
 * - O (Overflow): Unaffected
 *
 * Trap Conditions:
 * - STO (Stack Overflow) if no blocks of requested size or larger are available
 * - STO if requested log_size is larger than MAXL value in heap variables
 * - Addressing traps if TOS register or heap structure is invalid
 *
 * Performance:
 * - Best case: O(1) - block of exact size available
 * - Worst case: O(log N) - must split larger blocks recursively
 * - Typical: 15-30 CPU cycles for small blocks
 *
 * Typical Usage:
 *   ; Allocate 256-word buffer (log2(256) = 8)
 *   W1 GETB  #8          ; Allocate 256 words, address → I1
 *   W    (I1),BUFFER_DATA  ; Use allocated buffer
 *
 *   ; Allocate small 4-word structure
 *   W2 GETB  #2          ; Allocate 4 words, address → I2
 *
 *   ; Allocate large 16K word array
 *   W3 GETB  #14         ; Allocate 16384 words, address → I3
 *
 * Notes:
 * - This is a dynamic memory allocation operation
 * - Paired with PUTB (put block) to free allocated memory
 * - Block sizes must be power of 2 (buddy system requirement)
 * - Heap must be initialized before first GETB call
 * - TOS register must point to valid heap variable structure
 * - Allocated blocks are NOT zeroed (contain previous data)
 * - Thread-safe allocation requires SOLO instruction to disable interrupts
 * - Heap fragmentation is minimized by buddy coalescing
 *
 * IMPLEMENTATION STATUS: FULLY IMPLEMENTED
 *
 * This is a complete buddy system heap allocator implementation using:
 *
 * 1. Heap Manager Infrastructure:
 *    - TOS register points to heap variable structure
 *    - MAXL value read from heap variables (offset 0)
 *    - Freelist array accessed from heap variables (offset 1+)
 *    - Each freelist[k] contains blocks of size 2^k words
 *
 * 2. Buddy System Operations:
 *    - Searches freelists starting from requested size
 *    - Finds first available block in larger freelists if needed
 *    - Splits larger blocks recursively to requested size
 *    - Maintains buddy relationships through splitting
 *
 * 3. Memory Management:
 *    - Validates TOS register (heap initialized)
 *    - Checks log_size against MAXL (valid size range)
 *    - Raises STO trap on heap exhaustion
 *    - Raises STO trap on invalid size request
 *
 * 4. Block Splitting Algorithm:
 *    - Unlinks block from source freelist
 *    - Splits in half iteratively until reaching target size
 *    - Links unused buddies into appropriate freelists
 *    - Maintains freelist integrity throughout
 *
 * Note: PUTB (put block/free) would implement the reverse operation,
 * coalescing buddies when freeing blocks.
 *
 * Related Instructions:
 * - GETB: Get block from heap (allocate) [this instruction]
 * - PUTB: Put block to heap (free/deallocate)
 * - ENTS/ENTF: Enter subroutine (allocate stack frame)
 * - RET: Return from subroutine (deallocate stack frame)
 *
 * Reference: ND-500 Reference Manual, Chapter 4.1.5 (Buddy System)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/BITFIELD/Getb.cs
 */
void nd500_instr_Getb(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Verify operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] GETB expects 1 operand, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        return;
    }

    /* Verify target register is specified (W1-W4 prefix) */
    if (fi->target_register == 0 || fi->target_register > 4) {
        printf("[ERROR] GETB requires register prefix (W1-W4), got target_register=%u at PC=0x%08X\n",
               fi->target_register, fi->address);
        return;
    }

    /* Read log_size operand (byte value) - handles both register and memory modes */
    uint8_t log_size = (uint8_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);

    /* Read heap variables from TOS register */
    uint32_t heap_vars_addr = cpu->TOS;
    if (heap_vars_addr == 0) {
        printf("[ERROR] GETB at PC=0x%08X: TOS register is zero (heap not initialized)\n",
               fi->address);
        trap_stack_overflow(cpu, fi->address);
        return;
    }

    /* Read MAXL (maximum logarithmic size) from heap variables (word at offset +0) */
    uint32_t max_log = nd500_read_memory_32(cpu, heap_vars_addr + 0);

    /* Check if requested size exceeds maximum */
    if (log_size > max_log) {
        printf("[TRAP] GETB at PC=0x%08X: Requested log_size=%u exceeds MAXL=%u\n",
               fi->address, log_size, max_log);
        trap_stack_overflow(cpu, fi->address);
        return;
    }

    /* STEP 1: Check if exact size is available in FLOG[log_size] */
    uint32_t freelist_addr = heap_vars_addr + 12 + (log_size * 4);
    uint32_t block_addr = nd500_read_memory_32(cpu, freelist_addr);

    if (block_addr != 0) {
        /* Exact size available - unlink from free list */
        uint32_t next_block = nd500_read_memory_32(cpu, block_addr);
        nd500_write_memory_32(cpu, freelist_addr, next_block);
        printf("[GETB] Allocated exact block at 0x%08X from freelist[%u]\n", block_addr, log_size);
    } else {
        /* STEP 2: No exact size - search for larger blocks */
        uint8_t found_size = 0;
        for (uint8_t k = log_size + 1; k <= max_log; k++) {
            freelist_addr = heap_vars_addr + 12 + (k * 4);
            block_addr = nd500_read_memory_32(cpu, freelist_addr);

            if (block_addr != 0) {
                /* Found larger block - unlink it */
                uint32_t next_block = nd500_read_memory_32(cpu, block_addr);
                nd500_write_memory_32(cpu, freelist_addr, next_block);
                found_size = k;
                printf("[GETB] Found larger block at 0x%08X in freelist[%u], will split\n", block_addr, k);

                /* STEP 3: Split block repeatedly until we get requested size */
                while (found_size > log_size) {
                    found_size--;

                    /* Calculate size of half-block in words */
                    uint32_t half_block_size_words = (1U << found_size);
                    uint32_t half_block_size_bytes = half_block_size_words * 4;  /* Words to bytes */

                    /* Calculate address of second half (buddy) */
                    uint32_t buddy_addr = block_addr + half_block_size_bytes;

                    /* Link second half (buddy) to free list for this size */
                    uint32_t buddy_list_addr = heap_vars_addr + 12 + (found_size * 4);
                    uint32_t old_head = nd500_read_memory_32(cpu, buddy_list_addr);

                    /* Set buddy's next pointer to old head */
                    nd500_write_memory_32(cpu, buddy_addr, old_head);

                    /* Update FLOG[found_size] to point to buddy */
                    nd500_write_memory_32(cpu, buddy_list_addr, buddy_addr);

                    printf("[GETB] Split: kept 0x%08X, returned buddy 0x%08X to freelist[%u]\n",
                           block_addr, buddy_addr, found_size);
                }
                break;  /* Found and split block, exit loop */
            }
        }

        /* STEP 4: Check if allocation failed (no blocks available) */
        if (block_addr == 0) {
            /* No blocks available - trap STO */
            /* Note: STAH/ENDH are NOT used for allocation - they're only for initialization/trap handlers */
            printf("[TRAP] GETB at PC=0x%08X: No blocks available for log_size=%u\n",
                   fi->address, log_size);
            trap_stack_overflow(cpu, fi->address);
            return;
        }
    }

    /* Write allocated address to target register (I1-I4) */
    /* target_register is 1-4, array is 0-indexed */
    cpu->I[fi->target_register - 1] = block_addr;

    printf("[GETB] Block 0x%08X (size=2^%u words) allocated → I%u\n",
           block_addr, log_size, fi->target_register);

    /* Data status bits are unaffected per ND-500 Reference Manual §15.13 */
    /* NOTE: All flags (Z, S, C, K, O) remain unchanged */

    /* PC will be advanced automatically by cpu_step() */
}
