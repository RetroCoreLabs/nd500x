#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * GETB instruction - BITFIELD class
 *
 * Mnemonic: getb (with register prefix: W1, W2, W3, W4)
 * Operands: 1
 * Opcode: 0xFE4C+(n-1) (177114B+(n-1))
 *
 * Operation: Get buddy element - allocate element of size 2^<log size> words
 *            from the heap, address of element -> Wn
 *
 * Format: Wn GETB <log size/r/BY>
 *
 * Description (ND-500 Reference Manual section 15.13):
 * Allocate an element of size 2^<log size> words from the heap. If an element
 * of the given size is available, it is removed from the freelist and its
 * address is returned in the specified register. Otherwise the lists for
 * larger elements are examined. If a larger element is found, it is removed
 * from its freelist and chopped into halves until an element of the desired
 * size can be allocated; the other half of the chopped element(s) is added to
 * the appropriate freelists. If no larger element is available, or if the
 * requested size is larger than the MAXL value, a stack overflow (STO) trap
 * condition occurs.
 *
 * Heap variables (Reference Manual section 3.3, Figure 4), pointed to by TOS:
 *   TOS -> +0   MAXL        Max log size of elements allowed
 *          +4   STAH        Start of heap  (NOT used by heap instructions)
 *          +8   ENDH        End of heap    (NOT used by heap instructions)
 *          +12  FLOG0       Freelist head, elements of 2^0 words
 *          +16  FLOG1       Freelist head, elements of 2^1 words
 *          ...
 *          +12+4k FLOGk     Freelist head, elements of 2^k words
 *          ...  FLOG<MAXL>
 *
 * The first word of a free element contains the address of the next element
 * in the list; zero indicates the end of the list. A freelist head of zero
 * means no element of that log size is available.
 *
 * The heap variables must be initialized by the user program. STAH and ENDH
 * are reserved for a heap administration routine implemented as a trap
 * handler for the STO trap; GETB itself must never read or modify them.
 * On heap exhaustion GETB raises STO and leaves all state (freelists, Wn)
 * unchanged so the program's trap handler can seed or extend the heap and
 * the instruction can be re-executed.
 *
 * Trap conditions:
 * - Addressing traps
 * - Stack overflow (STO) if no element of the requested size or larger is
 *   available, or if <log size> is greater than MAXL
 *
 * Status bits:
 * - Data status bits (Z, S, C, K, O): unaffected (section 15.13)
 * - The STO status bit (ST1 bit 27) is set/reset for each ENTS, ENTSN, ENTB,
 *   INIT, ENTM and GETB instruction (Reference Manual section on traps):
 *   set when the overflow condition occurs, reset on successful completion.
 *
 * Example:
 *   ; Allocate a 64 word data block from the heap, address in W3
 *   W3 GETB 6
 *
 * Related instructions:
 * - FREEB (section 15.14): release element to the appropriate freelist
 *   (elements are not combined; that is left to the STO trap handler)
 * - ENTB/RETB/RETBK: subroutine entry/return with buddy-allocated local area
 *
 * Reference: ND-05.009.4 EN ND-500 Reference Manual, sections 3.3 and 15.13
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

    /* TOS must point to the heap variables */
    uint32_t heap_vars_addr = cpu->TOS;
    if (heap_vars_addr == 0) {
        printf("[ERROR] GETB at PC=0x%08X: TOS register is zero (heap not initialized)\n",
               fi->address);
        trap_stack_overflow(cpu, fi->address);
        return;
    }

    /* Read MAXL (maximum log size of elements allowed) at TOS+0 */
    uint32_t max_log = nd500_read_memory_32(cpu, heap_vars_addr + 0);

    /* Requested size larger than MAXL -> STO (section 3.3) */
    if (log_size > max_log) {
        TRACE("[GETB] PC=0x%08X: Requested log_size=%u exceeds MAXL=%u, invoking STO trap\n",
              fi->address, log_size, max_log);
        trap_stack_overflow(cpu, fi->address);
        return;
    }

    /* Exact size available in FLOG[log_size]? Unlink from the freelist. */
    uint32_t freelist_addr = heap_vars_addr + 12 + (log_size * 4);
    uint32_t block_addr = nd500_read_memory_32(cpu, freelist_addr);

    if (block_addr != 0) {
        uint32_t next_block = nd500_read_memory_32(cpu, block_addr);
        nd500_write_memory_32(cpu, freelist_addr, next_block);
    } else {
        /* List empty - examine the lists for larger elements */
        for (uint8_t k = log_size + 1; k <= max_log; k++) {
            freelist_addr = heap_vars_addr + 12 + (k * 4);
            block_addr = nd500_read_memory_32(cpu, freelist_addr);

            if (block_addr != 0) {
                /* Found larger element - unlink it */
                uint32_t next_block = nd500_read_memory_32(cpu, block_addr);
                nd500_write_memory_32(cpu, freelist_addr, next_block);

                /* Chop into halves until the desired size is reached; the
                 * upper half of each chop goes onto its freelist. Sizes are
                 * in words, addresses in bytes (1 word = 4 bytes). */
                uint8_t found_size = k;
                while (found_size > log_size) {
                    found_size--;

                    uint32_t half_block_size_bytes = (1U << found_size) * 4;
                    uint32_t buddy_addr = block_addr + half_block_size_bytes;

                    uint32_t buddy_list_addr = heap_vars_addr + 12 + (found_size * 4);
                    uint32_t old_head = nd500_read_memory_32(cpu, buddy_list_addr);

                    nd500_write_memory_32(cpu, buddy_addr, old_head);
                    nd500_write_memory_32(cpu, buddy_list_addr, buddy_addr);
                }
                break;
            }
        }

        /* No element of the requested size or larger available - raise STO
         * so the program's own trap handler can seed or extend the heap.
         * GETB itself must never touch STAH/ENDH (section 3.3: they are
         * reserved for trap handlers). Re-seeding from STAH here would hand
         * out blocks that overlap earlier allocations and corrupt live heap
         * objects. */
        if (block_addr == 0) {
            TRACE("[GETB] PC=0x%08X: No free blocks for log_size=%u, invoking STO trap\n",
                   fi->address, log_size);
            trap_stack_overflow(cpu, fi->address);
            return;
        }
    }

    /* Successful completion resets the STO status bit (set/reset for each
     * ENTS, ENTSN, ENTB, INIT, ENTM and GETB instruction). */
    cpu->ST1 &= ~(uint32_t)TRAP_STO;

    /* Write allocated address to target register (I1-I4) */
    /* target_register is 1-4, array is 0-indexed */
    cpu->I[fi->target_register - 1] = block_addr;

    /* Data status bits (Z, S, C, K, O) are unaffected per section 15.13 */

    /* PC will be advanced automatically by cpu_step() */
}
