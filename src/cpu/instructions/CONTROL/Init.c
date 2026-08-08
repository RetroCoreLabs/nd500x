#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Init instruction - CONTROL class
 *
 * Initialize stack for program startup.
 *
 * Mnemonic: INIT
 * Operands: 3 (bottom of stack, stack demand main, total stack demand)
 * Opcode: 0x00DC
 *
 * Format: INIT <<bottom of stack/r/W>>, <stack demand main/r/W>, <total stack demand/r/W>
 *
 * Operation:
 *   <<bottom of stack>> → B
 *   <<bottom of stack>> + <total stack demand> → TOS
 *   <<bottom of stack>> + <stack demand main> → B.SP
 *   0 → B.PREVB
 *   0 → B.RETA → L
 *
 * Description:
 *   Initialize a new stack for the main program or initial module.
 *   This is typically the first instruction executed when a program starts.
 *
 *   The zeros in PREVB and RETA serve as sentinels:
 *   - RET with PREVB=0 triggers stack underflow trap
 *   - Prevents returning past the bottom of the call stack
 *
 *   Stack overflow occurs if <stack demand main> >= <total stack demand>
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Stack overflow (STO) if stack demand main >= total stack demand
 *
 * Flags: Unaffected
 *
 * Reference:
 *   - ND-500 Reference Manual, Page 229 (Chapter 13.9)
 *   - Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CONTROL/Init.cs
 *
 * NOTE: Manual example on page 229 appears to have operands reversed.
 *       Correct: INIT FRAME, 0x1000, 0x10000 (main < total)
 *       Wrong:   INIT FRAME, 0x10000, 0x1000 (would trap!)
 */
void nd500_instr_Init(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Stack frame field offsets */
    const uint32_t OFFSET_PREVB = 0;   /* Previous B */
    const uint32_t OFFSET_RETA  = 4;   /* Return address */
    const uint32_t OFFSET_SP    = 8;   /* Stack pointer */

    /* Validate operand count */
    if (fi->operand_count != 3) {
        printf("[ERROR] INIT at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands (all should be WORD values) */
    uint32_t bottom_of_stack = nd500_read_operand_word(cpu, &fi->operands[0]);
    uint32_t stack_demand_main = nd500_read_operand_word(cpu, &fi->operands[1]);
    uint32_t total_stack_demand = nd500_read_operand_word(cpu, &fi->operands[2]);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }


    /* Check for stack overflow in specification (like C#) */
    if (stack_demand_main >= total_stack_demand) {
        ND500X_TRAPLOG("[TRAP] INIT at PC=0x%08X: Stack overflow - main demand 0x%08X >= total 0x%08X\n",
               fi->address, stack_demand_main, total_stack_demand);
        trap_stack_overflow(cpu, fi->address);
        return;
    }

    /* STEP 1: Set B to bottom of stack (like C# line 98) */
    cpu->B = bottom_of_stack;

    /* STEP 2: Set TOS (top of stack limit) (like C# line 101) */
    cpu->TOS = bottom_of_stack + total_stack_demand;

    /* STEP 3: Initialize stack frame header with sentinel values (like C# lines 104-106) */
    nd500_write_memory_32(cpu, cpu->B + OFFSET_PREVB, 0);  /* PREVB = 0 (bottom sentinel) */
    nd500_write_memory_32(cpu, cpu->B + OFFSET_RETA, 0);   /* RETA = 0 (bottom sentinel) */
    cpu->L = 0;  /* L also cleared */

    /* STEP 4: Set initial SP (like C# line 109) */
    nd500_write_memory_32(cpu, cpu->B + OFFSET_SP, bottom_of_stack + stack_demand_main);

    /* STO status bit is set/reset for each ENTS, ENTSN, ENTB, INIT, ENTM and
     * GETB (ND-500 Reference Manual, traps section). Successful completion
     * resets it - the bit must not stay stale after an earlier overflow. */
    cpu->ST1 &= ~(uint32_t)TRAP_STO;

    /* NOTE: C# code lines 113-124 copy arguments, but manual spec does NOT mention this.
     * INIT is executed at program startup BEFORE any CALL, so no arguments should exist.
     * We omit argument copying to strictly follow the manual specification.
     */

    /* Diagnostic only - gated so it never leaks onto the SINTRAN console.
     * Set ND500X_INITLOG=1 to see the stack setup each INIT performs. */
    if (getenv("ND500X_INITLOG")) {
        printf("[INIT] Stack initialized at B=0x%08X, TOS=0x%08X, SP=0x%08X\n",
               cpu->B, cpu->TOS, bottom_of_stack + stack_demand_main);
    }
}
