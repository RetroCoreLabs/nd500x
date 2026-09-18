/*
 * Retbk.c - ND-500 RETBK instruction (CALL class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * RETBK instruction - CALL class
 *
 * Mnemonic: retbk
 * Operands: 0
 * Opcode: 0xFE1D
 *
 * Operation: Return from buddy-allocated subroutine with K flag set
 *
 * Description:
 * Returns from a subroutine entered through ENTB, releasing the heap-allocated
 * local data area back to the buddy system free list. Identical to RETB except
 * the K flag in the status register is SET (K=1) instead of cleared.
 *
 * The K flag is used for conditional branching (IF K RET, IF K GO) and loop
 * control. Setting it on return allows the calling routine to test whether
 * the subroutine completed successfully or encountered an error condition.
 *
 * The allocated heap block is returned to the free list for reuse by other
 * routines. The block is linked to the appropriate free list according to
 * the size of the element (determined by log size stored in B.LOG).
 *
 * Elements are not combined automatically; buddy merging may be done by the trap
 * handler for the stack overflow trap condition when memory becomes fragmented.
 *
 * IMPORTANT: Routines entered through ENTB MUST return through RETB/RETBK (not RET/RETK),
 * otherwise heap blocks will leak.
 *
 * Operand Structure:
 * - No operands (register-only operation)
 *
 * Operation Steps:
 * 1. Validate operand count (must be 0)
 * 2. Get current block address from B register
 * 3. Read log size from B.LOG (offset 12)
 * 4. Read PREVB (offset 0) and RETA (offset 4) from stack frame
 * 5. Validate PREVB/RETA for stack underflow
 * 6. Free block back to heap:
 *    a. Read MAXL from heap variables (TOS + 0)
 *    b. Validate log size <= MAXL
 *    c. Link block to FLOG[log_size] free list
 * 7. Restore CPU registers (B, PC, L)
 * 8. Set K flag (RETBK sets, RETB clears)
 *
 * Heap Variables (pointed to by TOS register):
 * - Offset +0: MAXL (word, 4 bytes) - Maximum logarithmic size
 * - Offset +4: STAH (word, 4 bytes) - Start of heap address (unused by RETBK)
 * - Offset +8: ENDH (word, 4 bytes) - End of heap address (unused by RETBK)
 * - Offset +12: FLOG[0] - Freelist head for 2^0 word blocks (word, 4 bytes)
 * - Offset +16: FLOG[1] - Freelist head for 2^1 word blocks (word, 4 bytes)
 * - Offset +12+(k*4): FLOG[k] - Freelist head for 2^k word blocks
 *
 * Free List Structure:
 * - Simple linked list, one per block size
 * - First word of each free block = address of next block (0 = end of list)
 * - FLOG[k] points to first block of size 2^k words
 *
 * Stack Frame Structure (at B):
 * - B+0: PREVB (previous B register value)
 * - B+4: RETA (return address)
 * - B+8: SP (stack pointer)
 * - B+12: AUX/LOG (auxiliary field/log size for ENTB)
 *
 * Flag Behavior:
 * - K (Key/Invalid): Set to 1 (error condition signaled to caller)
 * - Z, S, C, O: Unaffected
 *
 * Trap Conditions:
 * - Stack underflow (STU) if PREVB == 0 (no previous frame)
 * - Addressing traps if B register or heap structure is invalid
 *
 * Typical Usage:
 *   ; Allocate heap block for local data
 *   W1 GETB #6         ; Allocate 64-word block, address -> I1
 *   CALL SUB1, 0       ; Call subroutine
 *   IF K GO ERROR      ; Branch if subroutine returned error
 *   ...
 *
 *   SUB1: ENTB         ; Enter using heap block (I1 -> B)
 *   ...
 *   IF (error) THEN
 *     RETBK            ; Return with K=1 (error)
 *   ELSE
 *     RETB             ; Return with K=0 (success)
 *   FI
 *
 * Notes:
 * - Pairs with ENTB (enter buddy subroutine)
 * - Block is NOT automatically merged with buddy (that's done by trap handler)
 * - Buddy merging is the responsibility of the STO trap handler
 * - RETBK sets K flag, RETB clears K flag
 * - Must be used for ENTB-entered routines to avoid heap leaks
 *
 * Related Instructions:
 * - ENTB: Enter buddy subroutine (use heap block for locals)
 * - RETB: Return from buddy subroutine and clear K
 * - RETBK: Return from buddy subroutine and set K [this instruction]
 * - GETB: Get block from heap (allocate)
 * - PUTB: Put block to heap (free)
 *
 * Reference: ND-500 Reference Manual, Chapter 3.3, Page 224
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Retbk.cs
 */
void nd500_instr_Retbk(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Stack frame field offsets */
    const uint32_t OFFSET_PREVB = 0;      /* Previous B */
    const uint32_t OFFSET_RETA  = 4;      /* Return address */
    const uint32_t OFFSET_SP    = 8;      /* Stack pointer (unused) */
    const uint32_t OFFSET_LOG   = 12;     /* Log size */

    /* Validate operand count */
    if (fi->operand_count != 0) {
        printf("[ERROR] RETBK expects 0 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* STEP 1: Get current block address from B register */
    uint32_t block_addr = cpu->B;

    if (block_addr == 0) {
        ND500X_TRAPLOG("[TRAP] RETBK at PC=0x%08X: Stack underflow (B=0)\n", fi->address);
        trap_stack_underflow(cpu, fi->address);
        return;
    }

    /* STEP 2: Read log size from B.LOG (offset 12) */
    /* CRITICAL: This is the log size (e.g., 6), NOT the block size (e.g., 64) */
    uint32_t log_size = nd500_read_memory_32(cpu, block_addr + OFFSET_LOG);

    /* STEP 3: Read return information from stack frame */
    uint32_t prev_b = nd500_read_memory_32(cpu, block_addr + OFFSET_PREVB);
    uint32_t ret_addr = nd500_read_memory_32(cpu, block_addr + OFFSET_RETA);

    /* STEP 4: Validate PREVB and RETA (check for stack underflow) */
    if (prev_b == 0 && ret_addr == 0) {
        /* Both zero - check if we should trap or switch domains */
        ND500X_TRAPLOG("[TRAP] RETBK at PC=0x%08X: Stack underflow (PREVB=0, RETA=0)\n", fi->address);
        trap_stack_underflow(cpu, fi->address);
        return;
    }

    /* STEP 5: Free block back to heap using buddy system (if heap is initialized) */
    uint32_t heap_vars_addr = cpu->TOS;
    if (heap_vars_addr != 0) {
        /* Read MAXL (maximum logarithmic size) from heap variables (word at offset +0) */
        uint32_t max_log = nd500_read_memory_32(cpu, heap_vars_addr + 0);

        /* Validate log size against maximum */
        if (log_size <= max_log) {
            /* Calculate freelist head address for size class log_size (FLOG starts at offset +12) */
            uint32_t freelist_addr = heap_vars_addr + 12 + (log_size * 4);

            /* Read current head of free list */
            uint32_t current_head = nd500_read_memory_32(cpu, freelist_addr);

            /* Link block to head of free list */
            /* block.NEXT = old head */
            nd500_write_memory_32(cpu, block_addr, current_head);

            /* FLOG[log_size] = block */
            nd500_write_memory_32(cpu, freelist_addr, block_addr);
        }
    }
    /* Note: If TOS=0 (heap not initialized), we still perform the return but skip heap operations */

    /* STEP 6: Restore CPU registers */
    cpu->B = prev_b;         /* Restore previous stack frame (B.PREVB -> B) */
    cpu->PC = ret_addr;      /* Jump to return address (B.RETA -> P) */
    cpu->L = ret_addr;       /* Update link register (B.RETA -> L) */

    /* STEP 7: SET K flag (RETBK sets, RETB clears) */
    nd500_set_flag(cpu, ND500_FLAG_K);

    /* Data status bits: K flag set, all others unaffected */
}
