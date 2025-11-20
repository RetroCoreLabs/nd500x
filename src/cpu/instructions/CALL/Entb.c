#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * ENTB instruction - CALL class
 *
 * Mnemonic: entb
 * Operands: 1
 * Opcode: 0x00BD (hex) / 0000275 (octal) / 189 (decimal)
 *
 * Operation: Enter Block (Enter subroutine with buddy heap allocation)
 *
 * Description:
 * Allocates a local data area from a heap using the buddy system allocator,
 * then enters a subroutine. The heap block serves as the stack frame for
 * the subroutine, with the B register pointing to the allocated block.
 *
 * This instruction is an alternative to ENTS that uses heap allocation instead
 * of stack allocation, allowing for dynamic-sized frames and avoiding stack
 * overflow when subroutines have large local data requirements.
 *
 * Buddy System Heap Structure:
 * The heap is described by variables pointed to by the TOS register:
 *   TOS + 0:  MAXL      - Max log size allowed
 *   TOS + 4:  STAH      - Start of heap address
 *   TOS + 8:  ENDH      - End of heap address
 *   TOS + 12: FLOG0     - Free list for 2^0 word blocks (4 bytes)
 *   TOS + 16: FLOG1     - Free list for 2^1 word blocks (8 bytes)
 *   TOS + 20: FLOG2     - Free list for 2^2 word blocks (16 bytes)
 *   ...
 *   TOS + (12 + 4*n): FLOG[n] - Free list for 2^n word blocks
 *
 * Free List Format:
 * - Simple linked list, one per block size
 * - First word of each free block = address of next block (0 = end of list)
 * - FLOG[n] points to first block of size 2^n words (4^n bytes)
 *
 * Block Allocation Algorithm (Buddy System):
 * 1. If exact size available in FLOG[logSize], unlink and return it
 * 2. If not, find larger block and split in half repeatedly
 * 3. Link unused halves (buddies) back to appropriate free lists
 * 4. If no blocks available or logSize > MAXL, raise STO trap
 *
 * Operand Structure:
 * - Operand[0]: Log size (read, byte)
 *   - Logarithm base 2 of block size in words
 *   - Block size = 2^logSize words = 2^(logSize+2) bytes
 *   - Example: logSize=4 → 2^4=16 words = 64 bytes
 *   - Addressing modes: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
 *   - Data type: Byte (BY prefix)
 *
 * Frame Layout (at allocated heap block address):
 * - B.PREVB   (+0):  Previous B register value (32-bit)
 * - B.RETA    (+4):  Return address (32-bit), also copied to L
 * - B.SP      (+8):  Stack pointer (inherited from old frame) (32-bit)
 * - B.AUX/LOG (+12): Log size of this block (32-bit)
 * - B.N       (+16): Argument count (32-bit)
 * - B.ARG1+   (+20): Argument effective addresses (array of 32-bit)
 *
 * Operation Steps:
 * 1. Validate operand count (must be 1)
 * 2. Validate instruction sequence (must follow CALL/CALLG)
 * 3. Read log size operand (byte value)
 * 4. Read MAXL from heap (TOS+0)
 * 5. Validate logSize <= MAXL
 * 6. Allocate block from heap using buddy system
 * 7. If allocation fails, raise STO trap
 * 8. Initialize frame:
 *    a. Write old B to PREVB (+0)
 *    b. Write return address to RETA (+4) and L register
 *    c. Inherit SP from old frame (+8)
 *    d. Write log size to AUX/LOG (+12)
 *    e. Write argument count to N (+16)
 *    f. Copy argument addresses to ARG1+ (+20)
 * 9. Update B register to allocated block address
 * 10. Clear pending call state
 *
 * Flag Behavior:
 * - All data status bits unaffected (S, Z, C, O, K)
 *
 * Trap Conditions:
 * - Addressing traps: Invalid address, page fault, protection violation
 * - Instruction Sequence Error (ISE): Not preceded by CALL/CALLG
 * - Stack Overflow (STO): logSize > MAXL or no heap blocks available
 *
 * Performance:
 * - Variable, depends on heap state and block splitting required
 * - Best case (exact size available): ~20 cycles
 * - Worst case (splitting large block): ~50+ cycles
 *
 * Key Characteristics:
 * - Heap-based subroutine entry with buddy allocation
 * - Dynamic-sized frames (2^logSize words)
 * - ISE trap if not preceded by CALL
 * - STO trap if heap exhausted or invalid size
 * - Frame structure identical to ENTS except AUX/LOG field
 * - Requires CALL/CALLG handshake (pending call state)
 * - Deallocation via RETB instruction
 *
 * Common Use Cases:
 * - Subroutines with large local data (avoid stack overflow)
 * - Dynamic data structures in subroutines
 * - Memory pools for temporary allocations
 * - Avoiding fixed stack size limitations
 *
 * Typical Usage:
 *   Example 1: Allocate 256-word (1KB) frame
 *     CALL MYSUB
 *     BY ENTB 8      ; 2^8 = 256 words = 1024 bytes
 *
 *   Example 2: Variable-sized allocation
 *     CALL BUFFER_SUB
 *     BY ENTB B.SIZE ; Size determined at runtime
 *
 *   Example 3: Small heap frame
 *     CALL HANDLER
 *     BY ENTB 2      ; 2^2 = 4 words = 16 bytes
 *
 * Notes:
 * - ENTB must be preceded by BY prefix (byte operand)
 * - Must follow CALL/CALLG instruction (validated)
 * - Paired with RETB instruction for proper frame deallocation
 * - Buddy system does not auto-merge on free (STO handler does that)
 * - TOS register must point to valid heap descriptor
 * - Heap must be initialized before first ENTB
 *
 * IMPLEMENTATION STATUS: TODO (Buddy system allocator not implemented)
 *
 * Current implementation:
 * - Full validation and error checking
 * - Comprehensive documentation from C# reference
 * - Frame initialization logic implemented
 * - Buddy allocator marked as TODO (requires ~250 lines)
 *
 * To complete:
 * 1. Implement buddy system heap management helpers:
 *    - nd500_read_heap_maxl()
 *    - nd500_allocate_heap_block()
 *    - nd500_split_heap_block()
 * 2. Add heap structure validation
 * 3. Add unit tests for buddy allocation
 *
 * Related Instructions:
 * - ENTS:  Enter stack-based subroutine (stack allocation)
 * - RETB:  Return from block subroutine (deallocate heap block)
 * - GETB:  Get heap block (explicit buddy allocation)
 * - FREEB: Free heap block (explicit buddy deallocation)
 * - CALL:  Call subroutine (sets up pending call state)
 * - CALLG: Call global subroutine (sets up pending call state)
 *
 * Reference: ND-500 Reference Manual, Chapter 3.3, Page 223
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Entb.cs
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructionset.BuddySystem.cs
 */
void nd500_instr_Entb(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* ========================================================================
     * STEP 1: VALIDATE OPERAND COUNT
     * ======================================================================== */
    if (fi->operand_count != 1) {
        printf("[ERROR] ENTB expects 1 operand, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* ========================================================================
     * STEP 2: VALIDATE INSTRUCTION SEQUENCE (must follow CALL/CALLG)
     * ======================================================================== */
    if (cpu->pending_call_return_address == 0) {
        printf("[ERROR] ENTB instruction sequence error - not preceded by CALL at PC=0x%08X\n",
               fi->address);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* ========================================================================
     * STEP 3: READ LOG SIZE OPERAND
     * ======================================================================== */
    /* Read log size (byte value - logarithm base 2 of block size in words) */
    uint32_t log_size = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);

    printf("[ENTB] Request: log_size=%u (2^%u = %u words = %u bytes) at PC=0x%08X\n",
           log_size, log_size, (1u << log_size), (1u << (log_size + 2)), fi->address);

    /* ========================================================================
     * STEP 4-6: BUDDY SYSTEM HEAP ALLOCATION
     * ========================================================================
     *
     * TODO: Implement buddy system heap management
     *
     * Required steps:
     * 1. Read MAXL from TOS+0 (max log size allowed)
     * 2. Validate log_size <= MAXL
     * 3. Try to allocate from FLOG[log_size] (exact size)
     * 4. If not available, find larger block and split
     * 5. Link split buddies back to free lists
     * 6. If no blocks available, raise STO trap
     *
     * Required helper functions (from Instructionset.BuddySystem.cs):
     *   uint32_t nd500_read_heap_maxl(Nd500Cpu* cpu);
     *   uint32_t nd500_read_heap_free_list_head(Nd500Cpu* cpu, uint32_t log_size);
     *   void nd500_write_heap_free_list_head(Nd500Cpu* cpu, uint32_t log_size, uint32_t addr);
     *   uint32_t nd500_allocate_heap_block(Nd500Cpu* cpu, uint32_t log_size);
     *   uint32_t nd500_split_heap_block(Nd500Cpu* cpu, uint32_t addr, uint32_t current_log, uint32_t target_log);
     *
     * Heap structure at TOS:
     *   +0: MAXL (max log size)
     *   +4: STAH (start of heap)
     *   +8: ENDH (end of heap)
     *   +12: FLOG[0] (free list for 2^0 words)
     *   +16: FLOG[1] (free list for 2^1 words)
     *   +20: FLOG[2] (free list for 2^2 words)
     *   ...
     *
     * Free list link format:
     *   - Each block's first word = address of next free block (0 = end)
     *
     * Allocation algorithm:
     *   1. Check FLOG[log_size] for exact size
     *   2. If found: unlink and return
     *   3. If not: search FLOG[log_size+1..MAXL] for larger block
     *   4. Split larger block in half repeatedly until reaching target size
     *   5. Link unused buddies to appropriate FLOG lists
     *
     * ======================================================================== */

    /* TEMPORARY STUB: For now, raise STO trap until buddy allocator is implemented */
    printf("[TODO] ENTB buddy allocator not implemented - raising STO trap at PC=0x%08X\n",
           fi->address);
    trap_stack_overflow(cpu, fi->address);
    return;

    /* ========================================================================
     * STEP 7-10: FRAME INITIALIZATION (will be enabled when allocator works)
     * ========================================================================
     *
     * This code is ready but commented out until buddy allocator exists:

    uint32_t block_address = allocated_block; // From buddy allocator

    // Frame offsets (identical to ENTS)
    const uint32_t OFFSET_PREVB = 0;
    const uint32_t OFFSET_RETA = 4;
    const uint32_t OFFSET_SP = 8;
    const uint32_t OFFSET_AUX_LOG = 12;
    const uint32_t OFFSET_N = 16;
    const uint32_t OFFSET_ARG1 = 20;

    // Save old B
    uint32_t old_b = cpu->B;

    // Initialize frame structure
    nd500_write_memory_32(cpu, block_address + OFFSET_PREVB, old_b);

    // Set return address in frame and L register
    uint32_t return_addr = cpu->pending_call_return_address;
    nd500_write_memory_32(cpu, block_address + OFFSET_RETA, return_addr);
    cpu->L = return_addr;

    // Inherit SP from old frame (oldB.SP -> B.SP)
    uint32_t old_sp = nd500_read_memory_32(cpu, old_b + OFFSET_SP);
    nd500_write_memory_32(cpu, block_address + OFFSET_SP, old_sp);

    // Store log size in AUX/LOG location
    nd500_write_memory_32(cpu, block_address + OFFSET_AUX_LOG, log_size);

    // Copy argument count
    uint32_t arg_count = cpu->pending_call_arg_count;
    nd500_write_memory_32(cpu, block_address + OFFSET_N, arg_count);

    // Copy argument addresses
    for (uint32_t i = 0; i < arg_count && i < 256; i++) {
        uint32_t arg_addr = cpu->pending_call_arg_addresses[i];
        nd500_write_memory_32(cpu, block_address + OFFSET_ARG1 + (i * 4), arg_addr);
    }

    // Update B register to new heap block
    cpu->B = block_address;

    // Clear pending call state (consumed by ENTB)
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    printf("[ENTB] Completed: B=0x%08X, log_size=%u, return=0x%08X, args=%u at PC=0x%08X\n",
           cpu->B, log_size, return_addr, arg_count, fi->address);

    // Data status bits unaffected

     * ======================================================================== */
}
