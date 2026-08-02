#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * FREEB instruction - SYSTEM class
 *
 * Mnemonic: freeb
 * Operands: 2
 * Opcode: 0xFDB6 (175666 octal)
 *
 * Operation: Free Block to Buddy System Heap - Add memory block to freelist
 *
 * Description:
 * Adds a data block to the buddy system heap. The block is prepended to
 * the freelist for the specified logarithmic size. This is the inverse
 * of GETB (get block) and is used both for:
 * - Heap initialization (adding initial memory to freelists)
 * - Freeing previously allocated blocks back to the heap
 *
 * Operand Structure:
 * - Operand[0]: log_size (read, byte) - logarithmic block size (0-31)
 * - Operand[1]: element (read, word) - address of block to add to freelist
 *
 * Block Size Calculation:
 * - block_size = 2^log_size words
 * - Example: log_size=10 -> block_size = 2^10 = 1024 words = 4096 bytes
 *
 * Operation Steps:
 * 1. Read log_size from operand[0] (byte value)
 * 2. Read element address from operand[1] (word value)
 * 3. Read TOS register (points to heap variables structure)
 * 4. Calculate freelist address: TOS + 12 + (log_size * 4)
 * 5. Read current freelist head from FLOG[log_size]
 * 6. Write old head to element's first word (element.next = old_head)
 * 7. Write element address to FLOG[log_size] (prepend to list)
 *
 * Heap Variables Structure (at TOS):
 * - Offset +0: MAXL (word) - Maximum logarithmic size
 * - Offset +4: STAH (word) - Start of heap address
 * - Offset +8: ENDH (word) - End of heap address
 * - Offset +12: FLOG[0] - Freelist head for 2^0 word blocks
 * - Offset +16: FLOG[1] - Freelist head for 2^1 word blocks
 * - ...
 * - Offset +12+(k*4): FLOG[k] - Freelist head for 2^k word blocks
 *
 * Flag Behavior:
 * - All flags unaffected
 *
 * Typical Usage:
 *   ; Initialize heap with 1024-word block at HEAP_START
 *   FREEB #10, HEAP_START     ; Add 2^10 word block to freelist
 *
 *   ; Free allocated block (after GETB)
 *   FREEB #8, I1              ; Return 256-word block to heap
 *
 * Related Instructions:
 * - GETB: Get block from heap (allocate)
 * - FREEB: Free block to heap (deallocate) [this instruction]
 * - RETB: Return from subroutine, frees stack frame
 *
 * Reference: ND-500 Reference Manual, Chapter 4.1.5 (Buddy System)
 */
void nd500_instr_Freeb(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Verify operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] FREEB expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        return;
    }

    /* Read operands */
    uint8_t log_size = (uint8_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
    uint32_t element = nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Read heap variables from TOS register */
    uint32_t heap_vars_addr = cpu->TOS;

    if (heap_vars_addr == 0) {
        /* TOS not initialized - silently do nothing (matches RETB behavior) */
        return;
    }

    if (element == 0) {
        /* Null pointer - nothing to free */
        return;
    }

    /* Read MAXL to validate log_size */
    uint32_t max_log = nd500_read_memory_32(cpu, heap_vars_addr + 0);

    if (log_size > max_log) {
        printf("[ERROR] FREEB at PC=0x%08X: log_size=%u exceeds MAXL=%u\n",
               fi->address, log_size, max_log);
        return;
    }

    /* Calculate freelist head address: FLOG starts at offset +12 */
    uint32_t freelist_addr = heap_vars_addr + 12 + (log_size * 4);

    /* Read current freelist head */
    uint32_t current_head = nd500_read_memory_32(cpu, freelist_addr);

    /* Link element to old head: element.next = current_head */
    nd500_write_memory_32(cpu, element, current_head);

    /* Update freelist head: FLOG[log_size] = element */
    nd500_write_memory_32(cpu, freelist_addr, element);

    if (getenv("ND500X_HEAPDBG")) {
        fprintf(stderr, "[HEAPDBG] FREEB PC=0x%08X log_size=%u element=0x%08X TOS=0x%08X\n",
                fi->address, log_size, element, cpu->TOS);
    }

    /* All flags unaffected per ND-500 Reference Manual */
    /* PC will be advanced automatically by cpu_step() */
}
