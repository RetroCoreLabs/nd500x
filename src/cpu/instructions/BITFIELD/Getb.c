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
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Allocate a block from the buddy heap (shared with ENTB). On failure the
     * helper has already raised the STO trap so the program's own handler can
     * seed/extend the heap. */
    uint32_t block_addr = 0;
    if (!nd500_heap_alloc_block(cpu, log_size, fi->address, &block_addr)) {
        return;
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
