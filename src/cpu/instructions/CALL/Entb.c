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
 * IMPLEMENTATION STATUS: implemented. Allocation uses the shared buddy-heap
 * helper nd500_heap_alloc_block() (also used by GETB); the frame is built as
 * in ENTS with the block log size stored in AUX/LOG and B.SP inherited from
 * the old frame. Covered by test/test_stack_overflow.c (tests 5-6).
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
     * STEP 4-6: BUDDY SYSTEM HEAP ALLOCATION (shared helper, also used by GETB)
     * ======================================================================== */
    /* On failure the helper has already raised STO so the program's own
     * stack-overflow trap handler can seed/extend the heap. */
    uint32_t block_address = 0;
    if (!nd500_heap_alloc_block(cpu, (uint8_t)log_size, fi->address, &block_address)) {
        return;
    }

    /* ========================================================================
     * STEP 7-10: FRAME INITIALIZATION
     * ========================================================================
     * Frame layout is identical to ENTS except that the block's log size is
     * stored in the AUX/LOG field, and B.SP is INHERITED from the old frame
     * (a heap block is not a stack extent, so SP is not derived from it). */
    const uint32_t OFFSET_PREVB   = 0;
    const uint32_t OFFSET_RETA    = 4;
    const uint32_t OFFSET_SP      = 8;
    const uint32_t OFFSET_AUX_LOG = 12;
    const uint32_t OFFSET_N       = 16;
    const uint32_t OFFSET_ARG1    = 20;

    uint32_t old_b = cpu->B;

    /* B.PREVB = old B */
    nd500_write_memory_32(cpu, block_address + OFFSET_PREVB, old_b);

    /* B.RETA = return address; also copied to L */
    uint32_t return_addr = cpu->pending_call_return_address;
    nd500_write_memory_32(cpu, block_address + OFFSET_RETA, return_addr);

    /* B.SP inherited from old frame (oldB.SP -> newBlock.SP) */
    uint32_t old_sp = nd500_read_memory_32(cpu, old_b + OFFSET_SP);

    /* A fault on that read leaves the value garbage; abort before it is used. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    nd500_write_memory_32(cpu, block_address + OFFSET_SP, old_sp);

    /* B.AUX/LOG = block log size */
    nd500_write_memory_32(cpu, block_address + OFFSET_AUX_LOG, log_size);

    /* B.N = argument count */
    uint32_t arg_count = cpu->pending_call_arg_count;
    nd500_write_memory_32(cpu, block_address + OFFSET_N, arg_count);

    /* B.ARG1+ = argument effective addresses */
    for (uint32_t i = 0; i < arg_count && i < ND500_MAX_OPERANDS; i++) {
        nd500_write_memory_32(cpu, block_address + OFFSET_ARG1 + (i * 4),
                              cpu->pending_call_arg_addresses[i]);
    }

    /* Update B to the new heap block frame */

    /* A memory fault on any access above must abort BEFORE the commit below: the
     * real machine loads L and releases the CALL/ENT* sequence interlock only in
     * the TERMINAL microword (MICRO-5800-B30 ENTS_END @004206 loads L via
     * D,DAC,REG05; ENTSN_3 @004254 asserts C,SEQ / INVSEQ), both alongside the
     * final WRITE and the exit to the next instruction. Earlier frame writes are
     * separate microwords, so a fault there leaves L and the interlock untouched
     * and the retried entry instruction still sees its CALL. Without this the
     * retry raises a FALSE ISE - the defect that killed vi through ENTS. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    cpu->B = block_address;

    /* L is a terminal-microword effect - committed here, not at the RETA write. */
    cpu->L = return_addr;

    /* Clear pending call state (consumed by ENTB) */
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    /* STO status bit is set/reset for each ENTS, ENTSN, ENTB, INIT, ENTM and
     * GETB (ND-500 Reference Manual, traps section). Successful completion
     * resets it - the bit must not stay stale after an earlier overflow. */
    cpu->ST1 &= ~(uint32_t)TRAP_STO;

    /* Data status bits (Z, S, C, O, K) unaffected */
}
